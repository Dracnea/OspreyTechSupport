#!/bin/bash
# Read or set VCCINT on an Osprey E335 through the controller API, verifying
# each module by readback.
#
#   ./e335-vccint.sh <box-ip>              # show all three modules
#   ./e335-vccint.sh <box-ip> 700          # set all three to 700 mV, verify
#   ./e335-vccint.sh <box-ip> 700 1        # set module 1 only
#
# Why per-module and verified: {"boardId":3} means "all modules", but the
# controller implements it as a loop that does I2C work (and a 500 ms sleep) on
# each board in turn. If anything else is on that bus the request can time out
# after only some modules were changed, leaving the box at MIXED rails with no
# error. So this sets one board at a time and reads each back.
#
# Range the controller accepts: 400-950 mV (5 mV steps from an 850 mV nominal).
# Stock default is 645 mV. Lower is more efficient but every bitstream has its
# own floor -- step down gradually and watch for errors/rejects.
set -uo pipefail
LOG="$(cd "$(dirname "$0")" && pwd)/e335-vccint.log"
exec > >(tee -a "$LOG") 2>&1

BOX=${1:?usage: $0 <box-ip> [mV] [module 0-2]}
MV=${2:-}
ONLY=${3:-}
API="http://$BOX:8200/controller"
TOL=15

info() {
    curl -fsS --max-time 30 "$API/getAllInfo" | python3 -c '
import json, sys
for f in json.load(sys.stdin)["FPGA"]:
    print(f["boardId"], f.get("voltage_vccint"), f.get("voltage_hbm"), f.get("chipTemp"))'
}
rail() { info | awk -v b="$1" '$1==b {print $2}'; }

if [ -z "$MV" ]; then
    echo "module  vccint_mV  hbm_mV  chip_C"
    info | awk '{printf "%6s  %9s  %6s  %6s\n", $1, $2, $3, $4}'
    rm -f "$LOG"; exit 0
fi

[ "$MV" -ge 400 ] && [ "$MV" -le 950 ] || { echo "refusing $MV mV: outside 400-950"; exit 1; }
BOARDS=${ONLY:-"0 1 2"}
fail=0
for b in $BOARDS; do
    ok=0
    for attempt in 1 2 3; do
        # voltage_hbm 0 = leave the HBM rail unchanged
        curl -fsS --max-time 90 -X POST "$API/setVoltage" -H 'Content-Type: application/json' \
             -d "{\"voltage_vccint\":$MV,\"voltage_hbm\":0,\"boardId\":$b}" >/dev/null \
            || echo "module $b: setVoltage attempt $attempt failed"
        sleep 4
        got=$(rail "$b")
        if [ -n "$got" ] && [ $(( got > MV ? got - MV : MV - got )) -le $TOL ]; then
            echo "module $b: $got mV (target $MV)"; ok=1; break
        fi
    done
    [ "$ok" = 1 ] || { echo "module $b: did NOT reach $MV mV (last read ${got:-none})"; fail=1; }
done
[ "$fail" = 0 ] && rm -f "$LOG"
exit $fail
