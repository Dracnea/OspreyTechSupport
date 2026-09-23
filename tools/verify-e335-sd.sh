#!/bin/bash
# Read-only check of a card written by flash-e335-sd.sh. Mounts both partitions
# read-only, changes nothing.
#
#   sudo DEV=/dev/sdX ./verify-e335-sd.sh
#   sudo DEV=/dev/sdX HOST=myname MAC=02:11:22:33:44:55 ./verify-e335-sd.sh   # also check these
set -uo pipefail
HERE=$(cd "$(dirname "$0")" && pwd)
LOG="$HERE/verify-e335-sd.log"
exec > >(tee -a "$LOG") 2>&1
echo "=================== $(date -Is) ==================="

DEV=${DEV:-}
HOST=${HOST:-}
MAC=${MAC:-}
fail=0
chk() { if eval "$2"; then printf '  PASS  %s\n' "$1"; else printf '  FAIL  %s\n' "$1"; fail=1; fi; }

[ "$(id -u)" -eq 0 ] || { echo "must run as root"; exit 1; }
[ -n "$DEV" ] && [ -b "${DEV}2" ] || { echo "set DEV=/dev/sdX (card with two partitions)"; exit 1; }

M=$(mktemp -d); B=$(mktemp -d)
trap 'umount "$M" 2>/dev/null; umount "$B" 2>/dev/null; rmdir "$M" "$B" 2>/dev/null' EXIT
mount -o ro "${DEV}2" "$M" || { echo "cannot mount ${DEV}2"; exit 1; }

echo "== identity =="
[ -n "$HOST" ] && chk "hostname is $HOST" 'test "$(cat "$M/etc/hostname" 2>/dev/null)" = "$HOST"'
chk "machine-id cleared"               'test ! -s "$M/etc/machine-id"'
chk "dbus machine-id cleared"          'test ! -s "$M/var/lib/dbus/machine-id"'
chk "ssh host key present"             'test -s "$M/etc/ssh/ssh_host_ed25519_key"'

echo "== network =="
chk "interfaces is dhcp"               'grep -q "iface eth0 inet dhcp" "$M/etc/network/interfaces"'
chk "interfaces == interfaces_backup"  'cmp -s "$M/etc/network/interfaces" "$M/etc/network/interfaces_backup"'
[ -n "$MAC" ] && chk "interfaces pins $MAC" 'grep -qi "hwaddress ether $MAC" "$M/etc/network/interfaces"'
echo "   eth0: $(grep -h hwaddress "$M/etc/network/interfaces" 2>/dev/null || echo 'no pinned MAC (updatemac decides)')"

echo "== system completeness =="
chk "init present"                     'test -f "$M/lib/systemd/systemd"'
chk "controller.service present"       'test -f "$M/etc/systemd/system/controller.service"'
# .wants entries are ABSOLUTE symlinks; inside a mounted image they resolve
# against THIS host's root, so re-root the target under the mount point.
W="$M/etc/systemd/system/multi-user.target.wants/controller.service"
chk "controller enabled at boot"       'test -L "$W" && test -f "$M$(readlink "$W")"'
chk "controller binary present"        'test -x "$M/opt/controller/controller"'
chk "xvc_server units present"         'test -f "$M/etc/systemd/system/xvc_server_1.service"'
chk "updatemac binary present"         'test -x "$M/opt/updatemac/updatemac"'
chk "web UI present"                   'test -f "$M/var/www/html/libraries.json"'
chk "swapfile present"                 'test -s "$M/swapfile"'
chk "no leftover flash state"          'test ! -f "$M/.flash-e335-sd.state"'

echo "== boot partition =="
if mount -o ro "${DEV}1" "$B" 2>/dev/null; then
    chk "BOOT.BIN on p1"               'ls "$B" | grep -qi "^BOOT.BIN$"'
    echo "   p1 contents: $(ls "$B" | tr "\n" " ")"
    umount "$B"
else
    echo "  FAIL  cannot mount ${DEV}1"; fail=1
fi

echo
echo "   rootfs used: $(du -sh "$M" 2>/dev/null | cut -f1)  of  $(df -h "$M" | awk 'NR==2{print $2}')"
umount "$M"
echo
if [ "$fail" -eq 0 ]; then
    echo "ALL CHECKS PASSED -- safe to move the card to the E335 and power on."
    rm -f "$LOG"
else
    echo "SOME CHECKS FAILED -- do not boot it yet. Log kept at $LOG"
    exit 1
fi
