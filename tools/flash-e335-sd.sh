#!/bin/bash
# Write a replacement SD card for an Osprey E335 from the images in ../firmware.
#
#   sudo DEV=/dev/sdX ./flash-e335-sd.sh
#
# Optional environment:
#   FW=<dir>          firmware directory (default: ../firmware next to this script)
#   HOST=<name>       hostname for the unit (default: arm, the stock name)
#   MAC=<mac>|keep    MAC to pin on eth0 (default: a random locally administered one;
#                     "keep" writes no hwaddress line and leaves it to /opt/updatemac)
#   ROOT_GIB=<n>      root partition size in GiB (default 8; the rootfs is ~1 GiB)
#   SWAP_MIB=<n>      swapfile size (default 512; the stock image used 2048)
#   COOLDOWN=<s>      idle seconds between chunks (default 30, see note 2)
#   YES=1             skip the "type the device name" confirmation
#
# Two constraints shape this script:
#
#  1. Every unit needs its OWN network identity. If two boxes boot with the same
#     pinned MAC they fight over one DHCP lease and flap the switch, so the card
#     gets a fresh MAC, fresh SSH host keys and an empty machine-id.
#
#  2. Cheap USB SD adapters can electrically DISCONNECT after 1.5-2 GB of
#     sustained writing ("usb X-Y: USB disconnect" in dmesg) and not come back
#     without a physical reseat. So the root filesystem is written in chunks with
#     a pause between them, and progress is recorded ON THE CARD: if the adapter
#     drops, reseat it and run the script again -- it resumes at the next chunk.
set -uo pipefail

HERE=$(cd "$(dirname "$0")" && pwd)
LOG="$HERE/flash-e335-sd.log"
exec > >(tee -a "$LOG") 2>&1
echo "=================== $(date -Is) ==================="

DEV=${DEV:-}
FW=${FW:-$HERE/../firmware}
HOST=${HOST:-arm}
MAC=${MAC:-}
ROOT_GIB=${ROOT_GIB:-8}
SWAP_MIB=${SWAP_MIB:-512}
COOLDOWN=${COOLDOWN:-30}
WORK=${WORK:-/var/tmp}
TAR="$WORK/.e335-rootfs.tar"       # decompressed once, kept between resumes
STATE=".flash-e335-sd.state"

die() {
    echo "ERROR: $*" >&2
    if [ -n "$DEV" ] && ! [ -b "$DEV" ]; then
        echo "--- $DEV IS GONE FROM THE BUS -- the adapter dropped." >&2
        echo "--- Reseat it and re-run: progress is saved on the card and the" >&2
        echo "--- script resumes from the next chunk." >&2
    fi
    echo "--- last kernel messages ---" >&2
    dmesg 2>/dev/null | tail -15 >&2
    echo "--- log kept at $LOG ---" >&2
    type restore_dirty >/dev/null 2>&1 && restore_dirty
    exit 1
}
alive() { [ -b "$DEV" ] || die "$DEV vanished from the bus"; }

echo "== guards =="
[ "$(id -u)" -eq 0 ]  || die "must run as root"
[ -n "$DEV" ]         || die "set DEV=/dev/sdX (the WHOLE card, not a partition)"
[ -b "$DEV" ]         || die "$DEV is not a block device"
val() { lsblk -dno "$1" "$DEV" 2>/dev/null | tr -d '[:space:]'; }
[ "$(val TYPE)" = "disk" ] || die "$DEV is not a whole disk"
[ "$(val RM)" = "1" ] || [ "$(val TRAN)" = "usb" ] || [ "$(val TRAN)" = "mmc" ] \
    || die "$DEV is neither removable nor USB/MMC -- refusing (is it a system disk?)"
case "$(findmnt -no SOURCE /)" in "$DEV"*) die "$DEV holds /";; esac
SZ=$(blockdev --getsize64 "$DEV") || die "cannot size $DEV"
NEED=$(( (ROOT_GIB + 1) * 1073741824 ))
[ "$SZ" -ge "$NEED" ] || die "$DEV is $SZ bytes; need at least $NEED for ROOT_GIB=$ROOT_GIB"
echo "   $DEV  $(val SIZE)  $(val TRAN)  model: $(lsblk -dno MODEL "$DEV")"
lsblk "$DEV"
if [ "${YES:-0}" != 1 ]; then
    read -r -p "Everything on $DEV will be destroyed. Type the device name to continue: " ans
    [ "$ans" = "$DEV" ] || die "not confirmed"
fi

if [ -z "$MAC" ]; then
    # locally administered, unicast: second-lowest bit of the first octet set
    MAC=$(printf '02:%02x:%02x:%02x:%02x:%02x' $((RANDOM%256)) $((RANDOM%256)) \
          $((RANDOM%256)) $((RANDOM%256)) $((RANDOM%256)))
fi
echo "   hostname=$HOST  mac=$MAC  root=${ROOT_GIB}GiB  swap=${SWAP_MIB}MiB"

# A big write builds gigabytes of dirty pages then flushes them in one burst,
# which is what knocks weak adapters off the bus. Cap dirty bytes while we run.
DIRTY_B=$(cat /proc/sys/vm/dirty_bytes 2>/dev/null || echo 0)
DIRTY_BG=$(cat /proc/sys/vm/dirty_background_bytes 2>/dev/null || echo 0)
restore_dirty() {
    echo "$DIRTY_B"  > /proc/sys/vm/dirty_bytes            2>/dev/null || true
    echo "$DIRTY_BG" > /proc/sys/vm/dirty_background_bytes 2>/dev/null || true
}
echo 33554432 > /proc/sys/vm/dirty_bytes            2>/dev/null || true
echo 16777216 > /proc/sys/vm/dirty_background_bytes 2>/dev/null || true

echo "== source image =="
( cd "$FW" && sha256sum -c SHA256SUMS ) || die "firmware checksum mismatch -- re-download"
if [ ! -s "$TAR" ]; then
    echo "   reassembling and decompressing the rootfs to $TAR (one time)"
    cat "$FW"/rootfs/e335-rootfs.tar.xz.part-* | xz -dc > "$TAR.tmp" \
        || die "rootfs decompress failed"
    mv "$TAR.tmp" "$TAR"
fi
echo "   rootfs tar ready: $(du -h "$TAR" | cut -f1)"
# Top-level entries, biggest first, so the hardest chunk runs while the adapter is cool.
CHUNKS=$(tar tvf "$TAR" | awk '{n=$6; sub(/^\.\//,"",n); split(n,a,"/"); if (a[1]!="") s[a[1]]+=$3}
                               END {for (k in s) print s[k], k}' | sort -rn | awk '{print $2}')

MNT=$(mktemp -d)
cleanup() { umount "$MNT" 2>/dev/null; rmdir "$MNT" 2>/dev/null; }
trap cleanup EXIT

MODE=fresh
if [ -b "${DEV}2" ] && mount "${DEV}2" "$MNT" 2>/dev/null; then
    [ -f "$MNT/$STATE" ] && MODE=resume
    umount "$MNT"
fi

if [ "$MODE" = resume ]; then
    echo "== RESUMING a previous run (state file found on ${DEV}2) =="
else
    echo "== partitioning: p1 512 MiB FAT32 boot, p2 ${ROOT_GIB} GiB ext4 root (stock offsets) =="
    alive
    for p in "$DEV"?*; do [ -b "$p" ] && umount "$p" 2>/dev/null; done
    udevadm settle || true
    wipefs -a "$DEV" || die "wipefs failed"
    sfdisk "$DEV" <<EOF || die "sfdisk failed"
label: dos
unit: sectors
${DEV}1 : start=8192,    size=1048576,                 type=b
${DEV}2 : start=1056768, size=$((ROOT_GIB*2097152)),   type=83
EOF
    partprobe "$DEV" 2>/dev/null || partx -u "$DEV" 2>/dev/null || true
    udevadm settle || true; sleep 2
    [ -b "${DEV}1" ] && [ -b "${DEV}2" ] || die "partitions did not appear"

    echo "== root filesystem =="
    alive
    # nodiscard skips a whole-device TRIM; -N caps inodes (~42k entries in the
    # rootfs). Both sharply cut the writes mkfs makes.
    mkfs.ext4 -F -L root -E nodiscard -N 200000 "${DEV}2" || die "mkfs.ext4 failed"
    sleep "$COOLDOWN"

    echo "== boot partition (byte for byte) =="
    alive
    xz -dc "$FW/boot/e335-boot-p1.img.xz" | dd of="${DEV}1" bs=1M conv=fsync status=none \
        || die "writing boot partition failed"
    sync; sleep "$COOLDOWN"

    alive
    mount "${DEV}2" "$MNT" || die "cannot mount ${DEV}2"
    printf 'created %s\n' "$(date -Is)" > "$MNT/$STATE"
    sync; umount "$MNT"
fi

echo "== root filesystem, in resumable chunks =="
alive
mount "${DEV}2" "$MNT" || die "cannot mount ${DEV}2"
for c in $CHUNKS; do
    grep -qx "chunk $c" "$MNT/$STATE" 2>/dev/null && { echo "   skip $c (done)"; continue; }
    echo "== extracting ./$c =="
    alive
    tar xf "$TAR" -C "$MNT" --numeric-owner -p "./$c" 2> >(head -20 >&2) \
        || die "extracting ./$c failed"
    sync || die "sync failed after ./$c -- writes did not reach the card"
    echo "chunk $c" >> "$MNT/$STATE"; sync
    alive
    echo "   ./$c done; cooling ${COOLDOWN}s"
    sleep "$COOLDOWN"
done

echo "== per-unit identity =="
alive
rm -f "$MNT"/etc/ssh/ssh_host_*
ssh-keygen -A -f "$MNT" >/dev/null || die "ssh-keygen -A failed"
: > "$MNT/etc/machine-id"
[ -f "$MNT/var/lib/dbus/machine-id" ] && : > "$MNT/var/lib/dbus/machine-id"
echo "$HOST" > "$MNT/etc/hostname" || die "cannot write /etc/hostname"
cat > "$MNT/etc/hosts" <<EOF || die "cannot write /etc/hosts"
127.0.0.1	localhost
127.0.1.1	$HOST.localdomain	$HOST

::1     localhost ip6-localhost ip6-loopback
ff02::1 ip6-allnodes
ff02::2 ip6-allrouters
EOF
# /opt/updatemac rewrites BOTH interfaces and interfaces_backup to this exact
# shape at boot whenever it changes the MAC. Writing the same shape into both
# means it has nothing to change and the pinned MAC survives.
for f in "$MNT/etc/network/interfaces" "$MNT/etc/network/interfaces_backup"; do
    {
        echo "source-directory /etc/network/interfaces.d"
        echo "auto eth0"
        echo "iface eth0 inet dhcp"
        [ "$MAC" = keep ] || echo "hwaddress ether $MAC"
    } > "$f" || die "cannot write $f"
done
sync || die "sync failed during identity step"

# Swapfile last: the card is already bootable, so a dropout here costs only this
# step. dd rather than fallocate: this kernel's swapon rejects unwritten extents.
if ! grep -qx "chunk swapfile" "$MNT/$STATE" 2>/dev/null; then
    echo "== swapfile (${SWAP_MIB} MiB) =="
    alive
    dd if=/dev/zero of="$MNT/swapfile" bs=1M count="$SWAP_MIB" status=none \
        || die "creating swapfile failed"
    chmod 600 "$MNT/swapfile"
    mkswap "$MNT/swapfile" >/dev/null || die "mkswap failed"
    sync || die "sync failed after swapfile"
    echo "chunk swapfile" >> "$MNT/$STATE"; sync
fi

echo "== verifying what was written =="
fail=0
grep -qx "$HOST" "$MNT/etc/hostname"                 || { echo "  BAD: hostname"; fail=1; }
grep -q "iface eth0 inet dhcp" "$MNT/etc/network/interfaces" \
                                                     || { echo "  BAD: not dhcp"; fail=1; }
if [ "$MAC" != keep ]; then
    grep -q "hwaddress ether $MAC" "$MNT/etc/network/interfaces"        || { echo "  BAD: MAC"; fail=1; }
    grep -q "hwaddress ether $MAC" "$MNT/etc/network/interfaces_backup" || { echo "  BAD: backup MAC"; fail=1; }
fi
[ -s "$MNT/etc/machine-id" ]                         && { echo "  BAD: machine-id not cleared"; fail=1; }
[ -f "$MNT/etc/ssh/ssh_host_ed25519_key" ]           || { echo "  BAD: no ssh host key"; fail=1; }
[ -f "$MNT/lib/systemd/systemd" ]                    || { echo "  BAD: no init -- rootfs incomplete"; fail=1; }
[ -f "$MNT/etc/systemd/system/controller.service" ]  || { echo "  BAD: controller.service missing"; fail=1; }
[ -x "$MNT/opt/controller/controller" ]              || { echo "  BAD: controller missing"; fail=1; }
[ -s "$MNT/swapfile" ]                               || { echo "  BAD: no swapfile"; fail=1; }
[ "$fail" -eq 0 ] || die "post-write verification failed -- do NOT boot this card"

rm -f "$MNT/$STATE"; sync
umount "$MNT"; rmdir "$MNT"; trap - EXIT
sync
restore_dirty
rm -f "$TAR"

echo
sfdisk -l "$DEV"
echo
echo "DONE -- move the card to the E335 and power it on."
[ "$MAC" = keep ] || echo "It will DHCP as $MAC; reserve an address for that MAC on your router if you want a fixed IP."
echo "Log in with: ssh ubuntu@<address>   (stock password: temppwd -- change it)"
rm -f "$LOG"
