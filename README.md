# OspreyTechSupport
Collection of DeObfuscated files related to Osprey FPGA miners

## Osprey E335 — backup and recovery kit

Everything needed to back up an Osprey E335, rebuild its SD card from scratch,
and understand the control board well enough to keep a unit running.

| directory | what's in it |
|---|---|
| [`firmware/`](firmware/) | A complete E335 SD card image — Osprey firmware **N2.0.30**, algorithms **A2.2.12**: the raw boot partition and the root filesystem, cleaned for sharing. |
| [`tools/`](tools/) | `flash-e335-sd.sh` writes a new card from `firmware/`; `verify-e335-sd.sh` checks it before you boot it; `e335-vccint.sh` reads/sets VCCINT with per-module readback; `e335_vrm_read.c` reads the regulators directly on the box. |
| [`bitstreams/`](bitstreams/) | Osprey's own E335 bitstreams (Astrix, Hoohash, Tari v1–v3) with loader-ready md5sum files. |
| [`control-board/`](control-board/) | Loose copies of Osprey's control-board software: controller, OTA updaters, miners and loaders, services, web UI, and the controller's C/C++ source. |
| [`docs/`](docs/) | [Hardware, access and the control board](docs/E335-overview.md) · [Voltage control](docs/E335-voltage-control.md) · [SD card failure, backup and recovery](docs/E335-sd-card-recovery.md) · [C1100 cooler](docs/C1100-cooler.md) |

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

## Osprey C1100 cooler (HeatSink-C1100)

Osprey's active cooler for the Varium C1100 / Alveo U55C has been delisted. Its
specs, its archived product pages and photos, and the screws it needs are
collected in [docs/C1100-cooler.md](docs/C1100-cooler.md).

| screw | size |
|---|---|
| inner (die clamp, spring-loaded) | **M3 × 10 mm** |
| outer (corner brackets) | **M2.5 × 10 mm** |

The holes are blind, so don't fit longer screws: a screw that bottoms out clamps
nothing.

