# Osprey E335 — voltage control

The E335's rails belong to the **Zynq controller**, not to the FPGA fabric. There
are three ways to set them: the web UI's voltage tab, the controller's HTTP API,
and — if the Osprey software is ever gone — speaking PMBus to the regulator
directly.

## 1. The controller API

```sh
# read back every module
curl -s http://<box-ip>:8200/controller/getAllInfo

# set VCCINT on module 0 to 700 mV, leave the HBM rail alone
curl -s -X POST http://<box-ip>:8200/controller/setVoltage \
     -H 'Content-Type: application/json' \
     -d '{"voltage_vccint":700,"voltage_hbm":0,"boardId":0}'
```

* `boardId` 0–2 targets one module; **`boardId: 3` (the module count) targets
  all three**.
* A rail passed as `0` is left at its current setting.
* The call persists into `/opt/controller/config/config.json`, so it survives a
  reboot.
* `getAllInfo` returns, per module, `voltage_vccint`, `voltage_hbm`, `chipTemp`,
  `boardTemp`, `mpsTemp` and the fan level.

[`../tools/e335-vccint.sh`](../tools/e335-vccint.sh) wraps this: it sets one
module at a time and reads each back.

### `boardId: 3` is not atomic

The controller implements "all modules" as a loop over the boards, doing I2C
work on each (and sleeping 500 ms per board between its two writes). If anything
else is using that I2C bus at the same time, the request can outlast an HTTP
timeout after only some modules were changed, leaving the box at **mixed rails
with no error reported**. It is not dangerous — the modules are independent —
but it silently invalidates any measurement that assumes a uniform rail.

So: **set each module individually and read the rail back** until it matches.

### Ranges

| Rail | Nominal | Accepted range | Stock default |
|---|---|---|---|
| VCCINT | 850 mV | **400 – 950 mV** | 645 mV |
| VCC_HBM | 1265 mV | **1000 – 1300 mV** | 1100 mV |

VCCINT is the rail that matters. Power scales with roughly V², so lowering
VCCINT is the biggest efficiency lever the box has: in our testing, taking
VCCINT from 800 mV down to 600 mV cut module power by nearly half at unchanged
hashrate. **Every bitstream has its own
floor, though** — dense, high-clock images need more voltage — so step down
gradually and watch the miner's error/reject counts at each step.

`getAllInfo` reports VCC_HBM around 978–983 mV, which is below the 1000 mV floor
the API accepts, and the rail draws essentially nothing when the bitstream does
not use HBM. Asking for the minimum would actually *raise* it slightly.

VCCAUX, VCCBRAM and VCCO are **not** exposed to the controller and cannot be
changed through it.

## 2. The wire protocol

From the controller's own source (`devices_controller.cpp`: `set_voltages()`,
`read_voltages()`, served out of `/var/www/html/`; copies in
[`../control-board/var/www/html/`](../control-board/var/www/html/)).

Each module's VRM is an **MPS dual-loop PMBus regulator at I2C address `0x7C`**,
on that module's PL I2C bus (`/dev/uio4`, `uio5`, `uio6` for FPGA0/1/2 — AXI IIC
cores at `0x41600000`, `0x41610000`, `0x41620000`).

**Write** (both rails move in one transaction, "tracking mode"):

1. `PAGE` (reg `0x00`) ← `0x02`
2. wait 500 ms
3. `VOUT_OFFSET` (reg `0x1E`) ← two bytes: `[vccint_step, vhbm_step]`

Each step is a signed offset from the rail's nominal in **5 mV units**, written as
one two's-complement byte:

| Rail | Step formula | Clamp |
|---|---|---|
| VCCINT | `(mV − 850) / 5` | −90 … +20 |
| VCC_HBM | `(mV − 1265) / 5` | −53 … +7 |

Example: 800 mV VCCINT is step −10 → `0xF6`.

**Read**: `PAGE` ← `0x00`, then `READ_VOUT` (`0x8B`) returns two bytes
little-endian; `(b1 << 8 | b0) & 0x0FFF` is VCCINT in millivolts. `PAGE` ← `0x01`
and the same read gives VCC_HBM. `READ_IOUT` (`0x8C`, amps) and `READ_POUT`
(`0x96`, watts) work on the same pages — the controller compiles these in but
exposes neither over HTTP. Pages 2 and 3 do not answer reads. Module temperature
is a separate device at `0x4D` (and `0x4E`).

That is everything an external controller needs: three I2C buses, address `0x7C`,
`PAGE`/`VOUT_OFFSET` to write and `PAGE`/`READ_VOUT` to verify. It does not
depend on the Osprey OS, its UI, or the closed controller binary.

[`../tools/e335_vrm_read.c`](../tools/e335_vrm_read.c) implements the **read**
side on the box (voltage, current and power per rail, plus a read-only I2C bus
scan). It is deliberately read-only, so it cannot set a wrong voltage.

Notes from using it:

* The controller polls the same bus, so an occasional transaction collides. Take
  a median of several samples, and do not read while a `setVoltage` is in flight.
* `READ_IOUT` / `READ_POUT` are integer amps and watts (~1 W resolution).
* The die temperature is a poor proxy for power on this box — a 56 % change in
  power moved it by about a degree. Use `READ_POUT`.
* A read-only scan of one module's bus also saw devices at `0x40` (a typical
  INA219/INA226 power-monitor address), `0x68` and `0x74`, none of which the
  controller names. The same scan missed the known VRM at `0x7C`, so treat the
  scan as unreliable until repeated.
