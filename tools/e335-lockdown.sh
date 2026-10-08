#!/bin/bash
# Close an Osprey E335 off from Osprey's back end: no OTA updates, no fleet
# "center management" phone-home, no remote.it tunnels. Run it ON the box, as root.
#
#   sudo ./e335-lockdown.sh            # apply (safe to run again)
#   sudo ./e335-lockdown.sh --check    # read-only audit, changes nothing
#   sudo ./e335-lockdown.sh --undo     # put the vendor services back
#
# From a PC:
#   scp e335-lockdown.sh ubuntu@<box>:/tmp/ && ssh -t ubuntu@<box> sudo bash /tmp/e335-lockdown.sh
#
# Optional environment:
#   SAMPLE=<s>     seconds of live-connection sampling in --check (default 60, 0 = skip)
#   HOSTS_BLOCK=0  don't pin the Osprey/remote.it/GitHub names to 127.0.0.1 in /etc/hosts
#
# What it changes, and why each one:
#
#  1. OTA flag off in both updaters, through their own local API
#     (127.0.0.1:8500/firmware, 127.0.0.1:8555/algorithm) -- the same switch as the web UI --
#     AND written directly as 0 to /opt/firmware/ota_status.txt and
#     /opt/algorithm/ota_status.txt, so it is set even when the updaters are not running.
#     THE SWITCH ALONE IS NOT ENOUGH: algorithm_update (A2.2.12) writes
#     /opt/algorithm/ota_status.txt = 0 but ignores it at startup and comes back
#     ENABLED after every reboot or restart. firmware_update (N2.0.30) does keep it.
#  2. firmware_update and algorithm_update stopped, disabled and masked -- this is what actually
#     holds. With OTA on they git-pull from GitHub every few minutes and can replace
#     the controller, miners and loaders and restart them.
#  3. client_handle stopped, disabled and masked: Osprey's "center management" agent, which
#     talks to center.ospreyelectronics.io. If that name ever comes back under
#     someone else's control, this is the path to your box.
#  4. connectd and connectd_schannel stopped, disabled and masked: remote.it agents that publish
#     the box's SSH (22) and web UI (80) to the internet, past your router's NAT.
#  5. The binaries above made non-executable, so nothing can start them by path.
#  6. apt's daily timer disabled. The image has no unattended-upgrades, so this only
#     stops pointless outbound traffic to a long-dead Ubuntu 16.04 mirror.
#  7. /etc/hosts pins github.com, the Osprey names and the remote.it names to
#     127.0.0.1, so even a revived agent cannot reach its back end. Nothing on the
#     box needs GitHub except the OTA updaters. The block is rewritten on every run.
#
# What it leaves alone: controller (:8200, the voltage/fan/temperature API),
# bridge_app, webserver/apache (web UI), xvc_server_*, the miners and loaders.
#
# Everything is reversible with --undo. Original unit files are kept in
# /etc/systemd/system/osprey-disabled/ and original binary modes in modes.txt there.
# --undo leaves the firmware OTA flag OFF; the algorithm updater turns its own back
# ON at start-up regardless (see 1.). To turn the firmware one back on:
#   curl -X POST -d '{"otaUpdateEnable":1}' http://127.0.0.1:8500/firmware/setOtaUpdate
#   curl -X POST -d '{"otaUpdateEnable":1}' http://127.0.0.1:8555/algorithm/setOtaUpdate
set -uo pipefail

MODE=apply
case "${1:-}" in
  ""|--apply) MODE=apply ;;
  --check)    MODE=check ;;
  --undo)     MODE=undo ;;
  -h|--help)  sed -n '2,44p' "$0"; exit 0 ;;
  *) echo "unknown option: $1 (use --check, --undo or nothing)"; exit 2 ;;
esac

[ "$(id -u)" = 0 ] || { echo "run as root: sudo $0 $*"; exit 1; }

LOG=/var/log/e335-lockdown.log
exec > >(tee -a "$LOG") 2>&1
echo "=================== $(date -Is) $(hostname) mode=$MODE ==================="

SAMPLE=${SAMPLE:-60}
HOSTS_BLOCK=${HOSTS_BLOCK:-1}
BK=/etc/systemd/system/osprey-disabled
UNITS="firmware_update algorithm_update client_handle connectd connectd_schannel"
TIMERS="apt-daily.timer apt-daily-upgrade.timer"
BINS="/opt/firmware/firmware_update /opt/algorithm/algorithm_update /opt/client_handle/client_handle
      /usr/bin/connectd.arm-linaro-pi /usr/bin/connectd_schannel.arm-linaro-pi"
OTA_APIS="8500/firmware 8555/algorithm"
OTA_FILES="/opt/firmware/ota_status.txt /opt/algorithm/ota_status.txt"
SINK_NAMES="github.com www.github.com codeload.github.com
            center.ospreyelectronics.io ospreyelectronics.io vptr.com dracaena.io
            remote.it www.remote.it remot3.it api.remot3.it fe1.remot3.it fe2.remot3.it
            fe3.remot3.it fe4.remot3.it mrtg.remot3.it api.weaved.com apilb.yoics.net
            chat18.prod.yoics.org chat19.prod.yoics.org chat20.prod.yoics.org"
MARK_BEGIN="# >>> e335-lockdown sinkhole >>>"
MARK_END="# <<< e335-lockdown sinkhole <<<"

unit_exists() { systemctl list-unit-files "$1.service" --no-legend 2>/dev/null | grep -q .; }

ota_set() {   # $1 = 0|1
  local ep r
  for ep in $OTA_APIS; do
    r=$(curl -s -m5 -H 'Content-Type: application/json' -X POST \
        -d "{\"otaUpdateEnable\":$1}" "http://127.0.0.1:$ep/setOtaUpdate")
    echo "  OTA $ep set $1 -> ${r:-no answer (service not running)}"
  done
}

report() {
  local ep u f e
  echo "-- OTA flags (from the updaters' API; 'not running' is expected once locked down)"
  for ep in $OTA_APIS; do
    echo "  $ep: $(curl -s -m5 "http://127.0.0.1:$ep/getOtaUpdate" || true)" | sed 's/: $/: not running/'
  done
  for f in $OTA_FILES; do [ -f "$f" ] && echo "  $f = $(cat "$f") (want 0)"; done
  echo "-- vendor services (want: masked / inactive)"
  for u in $UNITS; do
    e=$(systemctl is-enabled "$u" 2>/dev/null); printf '  %-20s %s / %s\n' "$u" "${e:-not installed}" "$(systemctl is-active "$u" 2>/dev/null)"
  done
  for u in $TIMERS; do
    unit_exists "${u%.timer}" || systemctl list-unit-files "$u" --no-legend 2>/dev/null | grep -q . || continue
    printf '  %-20s %s / %s\n' "$u" "$(systemctl is-enabled "$u" 2>/dev/null)" "$(systemctl is-active "$u" 2>/dev/null)"
  done
  echo "-- binaries (want: ---------- once locked down)"
  for f in $BINS; do [ -e "$f" ] && echo "  $(stat -c '%A' "$f") $f"; done
  echo "-- vendor processes still running"
  pgrep -af 'firmware_update|algorithm_update|client_handle|connectd' | grep -v -E 'e335-lockdown|pgrep' | sed 's/^/  /' || true
  pgrep -f 'firmware_update|algorithm_update|client_handle|connectd' >/dev/null || echo "  none"
  echo "-- remote.it registrations on this box"
  ls /etc/connectd/services/*.conf 2>/dev/null | sed 's/^/  /' || echo "  none"
  echo "-- /etc/hosts pins (want 127.0.0.1): $(grep -q "$MARK_BEGIN" /etc/hosts && echo present || echo absent)"
  for n in github.com center.ospreyelectronics.io; do echo "  $n -> $(getent hosts $n | awk '{print $1}' | head -1)"; done
  echo "-- services you need (want: active)"
  for u in controller xvc_server_1 xvc_server_2 xvc_server_3; do
    unit_exists "$u" && printf '  %-20s %s\n' "$u" "$(systemctl is-active "$u")"
  done
}

traffic() {
  local i
  echo "-- public IPs in the current syslog (top 30; LAN/loopback removed)"
  timeout 120 grep -a -h -o -E '\b([0-9]{1,3}\.){3}[0-9]{1,3}\b' /var/log/syslog 2>/dev/null \
    | grep -v -E '^(127\.|0\.|10\.|192\.168\.|255\.|172\.(1[6-9]|2[0-9]|3[01])\.|22[4-9]\.|23[0-9]\.|169\.254\.)' \
    | sort | uniq -c | sort -rn | head -30 | sed 's/^/  /'
  echo "-- SSH logins by source (127.0.0.1 would mean a remote.it tunnel was used)"
  grep -h -E 'Accepted ' /var/log/auth.log /var/log/auth.log.1 2>/dev/null \
    | grep -o -E 'from [0-9a-f.:]+' | sort | uniq -c | sed 's/^/  /'
  [ "$SAMPLE" -gt 0 ] || return 0
  echo "-- live connections to non-private addresses over ${SAMPLE}s"
  for i in $(seq 1 $((SAMPLE / 2))); do
    ss -tunp 2>/dev/null | awk 'NR>1{print $1, $6, $7}' \
      | grep -v -E ' (10\.|127\.|192\.168\.|172\.(1[6-9]|2[0-9]|3[01])\.|\[::1\]|\[::ffff:(10|127|192\.168)\.)'
    sleep 2
  done | sort | uniq -c | sed 's/^/  /'
  echo "  (sample done; nothing listed = no outside connections)"
  echo "  Look any address up with: whois <ip>  or  https://ipinfo.io/<ip>"
}

apply() {
  local u f m
  mkdir -p "$BK"
  echo "-- 1. OTA off"
  ota_set 0
  for f in $OTA_FILES; do
    [ -d "$(dirname "$f")" ] && echo 0 > "$f" && echo "  $f = 0"
  done
  echo "-- 2-4. stop, back up and mask vendor services"
  for u in $UNITS; do
    unit_exists "$u" || { echo "  $u: not installed"; continue; }
    systemctl stop "$u" 2>/dev/null
    systemctl disable "$u" >/dev/null 2>&1
    f=/etc/systemd/system/$u.service
    # systemd refuses to mask a unit whose real file lives in /etc/systemd/system,
    # so move that file into the backup directory first (once; never overwrite).
    if [ -f "$f" ] && [ ! -L "$f" ]; then
      if [ -e "$BK/$u.service" ]; then rm -f "$f"; else mv "$f" "$BK/"; fi
    fi
    systemctl mask "$u" >/dev/null 2>&1 && echo "  $u: stopped, masked" || echo "  $u: MASK FAILED"
  done
  [ -x /usr/bin/connectd_stop ] && /usr/bin/connectd_stop >/dev/null 2>&1
  pkill -f '/opt/firmware/firmware_update|/opt/algorithm/algorithm_update|/opt/client_handle/client_handle|connectd' 2>/dev/null
  systemctl daemon-reload
  systemctl reset-failed $UNITS 2>/dev/null
  echo "-- 5. binaries non-executable"
  for f in $BINS; do
    [ -e "$f" ] || continue
    # A mode of 0 means something already locked this file down; 755 is the safe original.
    m=$(stat -c '%a' "$f"); [ "$m" = 0 ] && m=755
    grep -q " $f\$" "$BK/modes.txt" 2>/dev/null || echo "$m $f" >> "$BK/modes.txt"
    chmod 000 "$f" && echo "  $f"
  done
  echo "-- 6. apt daily timers off"
  for u in $TIMERS; do
    systemctl list-unit-files "$u" --no-legend 2>/dev/null | grep -q . || continue
    systemctl disable --now "$u" >/dev/null 2>&1 && echo "  $u disabled"
  done
  if [ "$HOSTS_BLOCK" = 1 ]; then
    echo "-- 7. /etc/hosts: pin back-end names to 127.0.0.1"
    cp -n /etc/hosts "$BK/hosts.orig"
    sed -i "/^$MARK_BEGIN\$/,/^$MARK_END\$/d" /etc/hosts
    { echo "$MARK_BEGIN"; for n in $SINK_NAMES; do echo "127.0.0.1 $n"; done; echo "$MARK_END"; } >> /etc/hosts
    echo "  pinned $(echo $SINK_NAMES | wc -w) names"
  fi
}

undo() {
  local u f m
  echo "-- restore binaries"
  [ -f "$BK/modes.txt" ] && while read -r m f; do [ "$m" = 0 ] && m=755; [ -e "$f" ] && chmod "$m" "$f" && echo "  $m $f"; done < "$BK/modes.txt"
  echo "-- restore services"
  for u in $UNITS; do
    systemctl unmask "$u" >/dev/null 2>&1
    [ -f "$BK/$u.service" ] && [ ! -e "/etc/systemd/system/$u.service" ] && mv "$BK/$u.service" /etc/systemd/system/
  done
  systemctl daemon-reload
  for u in $UNITS; do
    unit_exists "$u" || continue
    systemctl enable "$u" >/dev/null 2>&1; systemctl start "$u" && echo "  $u started"
  done
  for u in $TIMERS; do
    systemctl list-unit-files "$u" --no-legend 2>/dev/null | grep -q . && systemctl enable --now "$u" >/dev/null 2>&1
  done
  echo "-- remove /etc/hosts pins"
  sed -i "/^$MARK_BEGIN\$/,/^$MARK_END\$/d" /etc/hosts
  echo "  firmware OTA left OFF (algorithm_update re-enables its own at start-up) -- see the script header."
}

case $MODE in
  check) report; traffic ;;
  apply) apply; echo; report ;;
  undo)  undo; sleep 10; echo; report ;;
esac
echo "log: $LOG"
