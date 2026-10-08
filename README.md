# OspreyTechSupport
Collection of DeObfuscated files related to Osprey FPGA miners

## Osprey E335 — backup and recovery kit

Everything needed to back up an Osprey E335, rebuild its SD card from scratch,
and understand the control board well enough to keep a unit running.

| directory | what's in it |
|---|---|
| [`firmware/`](firmware/) | A complete E335 SD card image — Osprey firmware **N2.0.30**, algorithms **A2.2.12**: the raw boot partition and the root filesystem, cleaned for sharing. |
| [`tools/`](tools/) | `flash-e335-sd.sh` writes a new card from `firmware/`; `verify-e335-sd.sh` checks it before you boot it; `e335-vccint.sh` reads/sets VCCINT with per-module readback; `e335_vrm_read.c` reads the regulators directly on the box; `e335-lockdown.sh` cuts the box off from Osprey's OTA, phone-home and remote.it services. |
| [`bitstreams/`](bitstreams/) | Osprey's own E335 bitstreams (Astrix, Hoohash, Tari v1–v3) with loader-ready md5sum files. |
| [`control-board/`](control-board/) | Loose copies of Osprey's control-board software: controller, OTA updaters, miners and loaders, services, web UI, and the controller's C/C++ source. |
| [`docs/`](docs/) | [Hardware, access and the control board](docs/E335-overview.md) · [Voltage control](docs/E335-voltage-control.md) · [SD card failure, backup and recovery](docs/E335-sd-card-recovery.md) · [Security: closing off Osprey's back end](docs/E335-security-hardening.md) · [C1100 cooler](docs/C1100-cooler.md) |

### Dead or dying SD card? Quick start

```sh
git clone https://github.com/Dracnea/OspreyTechSupport.git
cd OspreyTechSupport/tools
lsblk                                    # find the card, e.g. /dev/sdb
sudo DEV=/dev/sdb ./flash-e335-sd.sh
sudo DEV=/dev/sdb ./verify-e335-sd.sh
```

Put the card in the E335 and power it on. It comes up on DHCP; log in with
`ssh ubuntu@<address>` (password `temppwd` — change it), then install a bitstream
from `bitstreams/` and pick a miner in the web UI at `http://<address>/`.

### Lock the box down: no self-updates, no phone-home

Out of the box an E335 runs two OTA updaters that git-pull new firmware and miner
packages from GitHub and can restart the miner. It also runs Osprey's fleet agent,
and remote.it agents that expose its SSH and web UI to the internet through your
router. Osprey is gone, so nothing good can come through those paths. One script
shuts all of them off and leaves mining, voltage control and JTAG alone:

```sh
scp tools/e335-lockdown.sh ubuntu@<box-ip>:/tmp/
ssh -t ubuntu@<box-ip> sudo bash /tmp/e335-lockdown.sh --check   # audit only
ssh -t ubuntu@<box-ip> sudo bash /tmp/e335-lockdown.sh           # apply (--undo reverts)
```

Turning OTA off in the web UI is **not** enough on its own: the algorithm updater
turns itself back on after every reboot. See
[docs/E335-security-hardening.md](docs/E335-security-hardening.md) for details, a
router blocklist and ready-to-paste MikroTik rules.

## Osprey C1100 cooler (HeatSink-C1100)

Osprey's active cooler for the Varium C1100 / Alveo U55C has been delisted. Its
specs, its archived product pages and photos, and the screws it needs are
collected in [docs/C1100-cooler.md](docs/C1100-cooler.md).

| screw | size |
|---|---|
| inner (die clamp, spring-loaded) | **M3 × 10 mm only** |
| outer (corner brackets) | **M2.5 × 8 mm or × 10 mm** (the 8 mm option is for the outer screws only) |

The holes are blind, so go no longer than 10 mm: a screw that bottoms out clamps
nothing.

