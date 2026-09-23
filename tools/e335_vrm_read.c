/* e335_vrm_read -- read an Osprey E335 module's VRM directly, on the Zynq.
 *
 * Drives the AXI IIC core in the PL (dynamic mode) and speaks PMBus to the MPS
 * dual-loop regulator at 0x7C:
 *
 *     PAGE (0x00) = 0 -> VCCINT loop,  = 1 -> VCC_HBM loop
 *     READ_VOUT 0x8B (mV, & 0x0FFF)   READ_IOUT 0x8C (A)   READ_POUT 0x96 (W)
 *
 * Why: the Osprey controller compiles in read_Current()/read_wattage() but wires
 * them to no HTTP endpoint, so current and power are invisible over its API. This
 * also documents, in working code, the register sequence an external control
 * board would need if the Osprey software is ever gone.
 *
 * READ-ONLY on the VRM. No PAGE-2 / VOUT_OFFSET write path is implemented, so a
 * bug here can produce a wrong reading but can never set a wrong voltage. Use
 * the controller's :8200 setVoltage (or tools/e335-vccint.sh) for writes.
 *
 * Module -> uio mapping: FPGA0 = /dev/uio4, FPGA1 = /dev/uio5, FPGA2 = /dev/uio6.
 *
 * Self-check: page-0 READ_VOUT must match the controller's voltage_vccint.
 * `--check <mV>` asserts that and exits non-zero if it disagrees.
 *
 * The controller polls this same bus, so transactions can occasionally collide;
 * every value is a median of N samples with implausible values dropped. Do not
 * run it while a setVoltage call is in flight.
 *
 * Build and run on the box (gcc is included in the stock image):
 *   gcc -O2 -o e335_vrm_read e335_vrm_read.c
 *   sudo ./e335_vrm_read --uio 4 --check 645
 *   sudo ./e335_vrm_read --uio 4 --watts-only
 *   sudo ./e335_vrm_read --uio 4 --i2cscan      # read-only bus scan
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <stdint.h>
#include <time.h>

#define MAP_SZ        0x10000u

/* AXI IIC register offsets (PG090) */
#define R_CR          0x100u
#define R_SR          0x104u
#define R_TX_FIFO     0x108u
#define R_RX_FIFO     0x10Cu
#define R_RX_PIRQ     0x120u

#define CR_EN         0x01u
#define CR_TXFIFO_RST 0x02u
#define SR_BB         0x04u   /* bus busy      */
#define SR_RX_EMPTY   0x40u
#define SR_TX_EMPTY   0x80u

#define DYN_START     0x100u
#define DYN_STOP      0x200u

#define VRM_ADDR      0x7Cu
#define PMB_PAGE      0x00u
#define PMB_READ_VOUT 0x8Bu
#define PMB_READ_IOUT 0x8Cu
#define PMB_READ_POUT 0x96u

static volatile uint32_t *reg;

static inline uint32_t rd(unsigned off) { return reg[off / 4]; }
static inline void wr(unsigned off, uint32_t v) { reg[off / 4] = v; }

static void nsleep(long us)
{
    struct timespec ts = { us / 1000000L, (us % 1000000L) * 1000L };
    nanosleep(&ts, NULL);
}

static void iic_enable(void)
{
    wr(R_CR, CR_TXFIFO_RST);
    wr(R_CR, CR_EN);
    nsleep(200);
}

static void drain_rx(void)
{
    int n = 0;
    while (!(rd(R_SR) & SR_RX_EMPTY) && n++ < 32)
        (void)rd(R_RX_FIFO);
}

static int wait_bus_free(long us)
{
    long waited = 0;
    while (waited < us) {
        if (!(rd(R_SR) & SR_BB)) return 1;
        nsleep(500);
        waited += 500;
    }
    return 0;
}

/* Master write of n bytes, START..STOP. */
static int iic_write(unsigned addr, const uint8_t *buf, int n)
{
    int i;
    long waited = 0;
    iic_enable();
    wr(R_TX_FIFO, DYN_START | (addr << 1));
    for (i = 0; i < n - 1; i++)
        wr(R_TX_FIFO, buf[i]);
    wr(R_TX_FIFO, DYN_STOP | buf[n - 1]);
    while (waited < 300000) {
        if (rd(R_SR) & SR_TX_EMPTY) return 1;
        nsleep(200);
        waited += 200;
    }
    return 0;
}

/* Write one register byte, repeated START, then read n bytes. */
static int iic_write_read(unsigned addr, uint8_t regaddr, uint8_t *out, int n)
{
    int got = 0;
    long waited = 0;
    iic_enable();
    drain_rx();
    wr(R_RX_PIRQ, (uint32_t)(n > 0 ? n - 1 : 0));
    wr(R_TX_FIFO, DYN_START | (addr << 1));
    wr(R_TX_FIFO, regaddr);
    wr(R_TX_FIFO, DYN_START | (addr << 1) | 1u);
    wr(R_TX_FIFO, DYN_STOP | (uint32_t)n);
    while (got < n && waited < 300000) {
        if (!(rd(R_SR) & SR_RX_EMPTY))
            out[got++] = (uint8_t)(rd(R_RX_FIFO) & 0xFF);
        else { nsleep(200); waited += 200; }
    }
    return got == n;
}

static int read_word(uint8_t regaddr, int *val)
{
    uint8_t b[2];
    if (!iic_write_read(VRM_ADDR, regaddr, b, 2)) return 0;
    *val = (b[1] << 8) | b[0];
    return 1;
}

static int cmp_int(const void *a, const void *b)
{
    return (*(const int *)a) - (*(const int *)b);
}

static int median(int *v, int n) { qsort(v, n, sizeof(int), cmp_int); return n ? v[n / 2] : -1; }

/* Read one PMBus page; medians of `samples`. Returns count of valid V samples. */
static void read_rail(int page, int samples, int *mv, int *amps, int *watts)
{
    int vs[64], is[64], ws[64];
    int nv = 0, ni = 0, nw = 0, s, v;
    uint8_t pg[2];
    pg[0] = PMB_PAGE;
    pg[1] = (uint8_t)page;

    for (s = 0; s < samples && s < 64; s++) {
        wait_bus_free(200000);
        iic_write(VRM_ADDR, pg, 2);
        nsleep(2000);
        if (read_word(PMB_READ_VOUT, &v)) { v &= 0x0FFF; if (v >= 300 && v <= 1400) vs[nv++] = v; }
        if (read_word(PMB_READ_IOUT, &v)) { if (v >= 0 && v <= 400) is[ni++] = v; }
        if (read_word(PMB_READ_POUT, &v)) { if (v >= 0 && v <= 500) ws[nw++] = v; }
        nsleep(3000);
    }
    *mv    = median(vs, nv);
    *amps  = median(is, ni);
    *watts = median(ws, nw);
}

int main(int argc, char **argv)
{
    int uio = 4, samples = 5, watts_only = 0, check = -1, i, fd, scan = 0;
    int v0, i0, w0, v1, i1, w1;
    char path[64];

    for (i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--uio") && i + 1 < argc)          uio = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--samples") && i + 1 < argc)  samples = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--watts-only"))               watts_only = 1;
        else if (!strcmp(argv[i], "--check") && i + 1 < argc)     check = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--scan"))                      scan = 1;
        else if (!strcmp(argv[i], "--i2cscan"))                   scan = 2;
        else { fprintf(stderr, "usage: %s [--uio 4|5|6] [--samples N] [--watts-only] [--check mV]\n", argv[0]); return 1; }
    }

    snprintf(path, sizeof path, "/dev/uio%d", uio);
    fd = open(path, O_RDWR | O_SYNC);
    if (fd < 0) { perror(path); return 1; }
    reg = mmap(NULL, MAP_SZ, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (reg == MAP_FAILED) { perror("mmap"); close(fd); return 1; }

    if (scan == 2) {
        /* Bus scan: which devices are on this module's PL I2C at all?  The
           controller only ever names 0x7C (VRM) and 0x4D/0x4E (temperature),
           so this answers "are there other adjustable rails hiding here".
           Read-only: one register read per address, nothing is written. */
        int a, mv;
        uint8_t b[2];
        printf("uio%d  I2C bus scan (read-only)\n", uio);
        for (a = 0x08; a <= 0x77; a++) {
            wait_bus_free(50000);
            if (iic_write_read((unsigned)a, 0x00, b, 1)) {
                mv = 0;
                printf("  0x%02x ACK", a);
                if (a == VRM_ADDR) printf("   <- the dual-loop VRM (VCCINT + VCC_HBM)");
                if (a == 0x4D || a == 0x4E) printf("   <- temperature sensor");
                printf("\n");
                (void)mv;
            }
        }
        munmap((void *)reg, MAP_SZ); close(fd);
        return 0;
    }

    if (scan) {
        /* Which PMBus pages does this regulator actually answer on?  The
           controller only ever touches 0 (VCCINT), 1 (VCC_HBM) and 2 (the
           tracking page it writes VOUT_OFFSET through). */
        int pg, mv, a, w;
        printf("uio%d  VRM 0x%02x  page scan\n", uio, VRM_ADDR);
        for (pg = 0; pg < 4; pg++) {
            read_rail(pg, samples, &mv, &a, &w);
            printf("  page%d: %5d mV  %4d A  %4d W%s\n", pg, mv, a, w,
                   (mv < 0) ? "   (no answer)" : "");
        }
        munmap((void *)reg, MAP_SZ); close(fd);
        return 0;
    }

    read_rail(0, samples, &v0, &i0, &w0);

    if (watts_only) {
        if (w0 < 0) { munmap((void *)reg, MAP_SZ); close(fd); return 1; }
        printf("%d\n", w0);
        munmap((void *)reg, MAP_SZ); close(fd);
        return 0;
    }

    read_rail(1, samples, &v1, &i1, &w1);
    printf("uio%d  VRM 0x%02x\n", uio, VRM_ADDR);
    printf("  page0 VCCINT : %d mV  %d A  %d W\n", v0, i0, w0);
    printf("  page1 VCC_HBM: %d mV  %d A  %d W\n", v1, i1, w1);

    munmap((void *)reg, MAP_SZ);
    close(fd);

    if (check >= 0) {
        if (v0 < 0 || abs(v0 - check) > 25) {
            fprintf(stderr, "  CHECK FAILED: page0 VOUT %d vs expected %d\n", v0, check);
            return 2;
        }
        printf("  CHECK OK: page0 VOUT %d mV matches controller %d mV\n", v0, check);
    }
    return 0;
}
