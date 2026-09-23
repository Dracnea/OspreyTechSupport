# Osprey E335 — hardware, access and the control board

Everything here was measured on real E335 units running Osprey firmware
**N2.0.30** with algorithm package **A2.2.12**, the latest release we
have seen.

## What the box is

An ARMv7 **Zynq-7000 control board** (`Linux 5.4.0-xilinx`, hostname `arm`,
Ubuntu 16.04 userland) carrying **three Xilinx Virtex UltraScale+ `xcvu35p`
modules**. The FPGAs are never plugged into a host PC; everything reaches them
through the Zynq.

* The modules are **plain VU35P**: JTAG IDCODE `0x14b71093`. The CIV variant
  would read `0x14b6d093`.
* The controller's `getAllInfo` and `config.json` both report
  `"chipType": "vu35p_civ"`. **That string is a static vendor default, not a
  device read**, so do not use it to identify your part. Read the IDCODE instead.
* Osprey's own E335 bitstreams target `xcvu35p-fsvh2104-2LV-e` (some are built for
  `-2L-e`); see [`../bitstreams/README.md`](../bitstreams/README.md).

## Access

| What | How |
|---|---|
| Shell | `ssh ubuntu@<box-ip>`, stock password `temppwd`. `sudo` takes the same password. |
| Web UI | `http://<box-ip>/` — titled "Osprey Electronics \| E300 Miner". Every UI action POSTs to one CGI, `/cgi-bin/qcmap_web_cgi.cgi`, dispatched by a `Page=` parameter. |
| Controller API | `http://<box-ip>:8200` — `GET /controller/getAllInfo`, `POST /controller/setVoltage`, `POST /controller/removeBistreamFile`. |

## Topology

Each FPGA has its **own** JTAG core and its **own** I2C bus in the Zynq PL:

| Module | JTAG core (`axi_jtag`) | uio | XVC port | I2C core (VRM + temp) | uio |
|---|---|---|---|---|---|
| FPGA0 | `0x43c00000` | `/dev/uio2` | **2542** | `0x41600000` | `/dev/uio4` |
| FPGA1 | `0x43c10000` | `/dev/uio3` | **2543** | `0x41610000` | `/dev/uio5` |
| FPGA2 | `0x43c20000` | `/dev/uio1` | **2541** | `0x41620000` | `/dev/uio6` |

Other PL blocks: `axi_pwm_top` (fans, uio0), `axi_dna` (uio11), `axi_ping`
(uio12), three `gpio` (uio7/13/14), and three `serial` (uio8/9/10 — one UART per
module).

**It is not a three-device JTAG chain.** Each `axi_jtag` core drives exactly one
FPGA on its own single-device chain, so no IR padding or multi-device handling is
needed.

The VU35P is a 2-SLR SSI part, so its **IR is 12 bits**, not 6: the user opcodes
are `master << 6 | 0x24` — USER1 `0xA4`, USER2 `0xE4`, USER3 `0x8A4`.

### Clocks

Four 100 MHz differential inputs were measured on every module (all three are
wired the same):

| pins | bank | measured |
|---|---|---|
| F13/F12 | 67_L11 | 100.005 MHz |
| **BB18/BC18** | **64_L11** | **100.005 MHz** — the one the vendor images use |
| BD23/BD24 | 65_L11 | 100.001 MHz |
| BC32/BC33 | 66_L11 | 99.999 MHz |

## XVC (network JTAG)

Three `xvc_server` units ship on the image but **disabled**. Start them with:

```sh
sudo systemctl start xvc_server_1 xvc_server_2 xvc_server_3
```

They then serve standard XVC 1.0 (`getinfo:` → `xvcServer_v1.0:2048`, i.e. a
2048-byte maximum shift) on `0.0.0.0`, so Vivado Hardware Manager or
openFPGALoader on another machine on your LAN can reach each module directly.

**The XVC servers and the miners contend for the same `/dev/uio*` JTAG core**, so
stop any running miner service before using XVC, and stop XVC before starting a
miner.

## Loading bitstreams

Three routes, in increasing order of independence from the vendor software:

1. **Vendor loader** — each `/opt/<algo>/` ships a `loadall<algo>` ARM binary that
   programs all three modules from `/opt/<algo>/bits/`. Bit names are fixed
   (`e335_v1.bit`, `e335_v2.bit`…) and the loader picks the one for the board.
   **It verifies `<name>.bit.md5sum` and deletes the bitstream on mismatch**, so
   always write the md5sum file alongside. A good load prints
   `End of status: HIGHHHHHHH` once per module.
2. **Web UI upload** — the firmware page posts `Page=dracaUploadBit` with a
   multipart file.
3. **Over XVC** — any XVC-capable programmer can configure a module with no
   ARM-side tooling. This is the route that survives the vendor software.

If the SD card is suspect (see [`E335-sd-card-recovery.md`](E335-sd-card-recovery.md)),
stage bitstreams in RAM rather than on the card:

```sh
sudo systemctl stop xvc_server_1 xvc_server_2 xvc_server_3
sudo mount -t tmpfs -o size=128M,mode=0777 tmpfs /opt/<algo>/bits
#   copy the .bit in, then:  (cd /opt/<algo>/bits && md5sum e335_v1.bit > e335_v1.bit.md5sum)
sudo /opt/<algo>/loadall<algo>
```

## The miner registry

The web UI's miner page is driven by `/var/www/html/libraries.json`, the registry
of selectable miners. Each entry carries a dev fee and, for miners the UI launches
directly, a `run_command` template plus a `run_args` index list:

```json
"ironfish": {
  "fee": 0, "developers": ["Osprey"], "useHBM": 1, "useKey": 0,
  "run_command": "sudo /opt/ironfish/ironfish -o %s -u %s.%s --max-temp=85 -t %s,%s,%s -p 4 --queue_size=115 --clock_axi=%s --clock_hbm=120",
  "run_args": [3,4,5,6,7,8,9]
}
```

So adding a miner is a registry entry plus a directory: put the binary and a
`bits/` directory under `/opt/<name>/`, add a `libraries.json` entry, and it
becomes selectable. The per-algorithm `start<algo>.sh` scripts end with a JSON
comment line that the vendor tooling parses ("PLEASE DO NOT REMOVE/EDIT") — keep
that shape if you write one. `/opt/wala/sample.sh` is a clean template.

Osprey-developed miners in N2.0.30 / A2.2.12, with the dev fee the registry lists:

| miner | directory | fee |
|---|---|---|
| Pyrin | `/opt/pyrin` | 10 % |
| Iron Fish | `/opt/ironfish` | 0 % |
| Astrix | `/opt/astrix` | 0 % |
| Wala | `/opt/wala` | 10 % |
| Tari | `/opt/tari_os` | 10 % |
| Hoohash | `/opt/hoohash` | 10 % |
| Iron Fish + Tari dual | `/opt/ironfish_tari_os` | 0 % |
| Vecno | `/opt/vecno_os` | 10 % |

## Other on-board software

| path | what it is |
|---|---|
| `/opt/controller/controller` | the controller daemon: voltages, fans, temperatures, the `:8200` API. `controller_origin` is an older build. |
| `/opt/controller/config/` | `config.json` (live), `config_default.json` (factory) |
| `/opt/firmware/firmware_update` | OTA firmware updater; `firmware_version.txt` holds `N2.0.30` |
| `/opt/algorithm/algorithm_update` | OTA miner-package updater; `algorithm_version.txt` holds `A2.2.12` |
| `/opt/bridge_app`, `/opt/client_handle` | Osprey's management/bridge services |
| `/opt/updatemac/updatemac` | sets eth0's MAC at boot and rewrites `/etc/network/interfaces` + `interfaces_backup` |
| `/opt/xvc_server/xvc_server` | the XVC server |
| `/opt/services/*.service` | the unit files the OTA installs into `/etc/systemd/system` |
| `/var/www/html/` | the web UI. It also serves the controller's own C/C++ source (`devices_controller.cpp`, `iic_function.cpp`, `config.cpp`), which is how the voltage protocol below was worked out. |
| `/opt/scripts` | the public BeagleBoard `boot-scripts` repo, unmodified |

Loose copies of all of the above are in [`../control-board/`](../control-board/).

## Temperature protection

The controller reads module temperature over I2C (`0x4D`/`0x4E`) and applies
`temps_max` from `config.json`. If you run your own bitstreams, build them with
`BITSTREAM.CONFIG.OVERTEMPSHUTDOWN Enable` so the die protects itself even if
the controller is not running. If you replace the stock fans with externally
powered ones, the controller's fan readings and fan control become meaningless.
