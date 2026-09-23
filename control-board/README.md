# Osprey E335 control-board files

Loose, browsable copies of Osprey's own software from the Zynq control board,
taken from the same cleaned image as [`../firmware/`](../firmware/) — so they are
exactly what a restored card contains. Paths mirror the card's filesystem.

| path | contents |
|---|---|
| `opt/controller/` | controller daemon (`controller`, older `controller_origin`) and its `config/` (live `config.json` reset to factory values, `config_default.json`) |
| `opt/firmware/` | `firmware_update` OTA tool, `firmware_version.txt` = N2.0.30 |
| `opt/algorithm/` | `algorithm_update` OTA tool and `algo_config.json`, `algorithm_version.txt` = A2.2.12 |
| `opt/<miner>/` | Osprey miners: `astrix`, `hoohash`, `ironfish`, `ironfish_tari_os`, `pyrin`, `tari_os`, `vecno_os`, `wala` — each with its `loadall<miner>` bitstream loader. `bits/` directories are empty; bitstreams are in [`../bitstreams/`](../bitstreams/). |
| `opt/bridge_app/`, `opt/client_handle/` | Osprey management/bridge services |
| `opt/updatemac/` | boot-time MAC assignment |
| `opt/networking/` | DHCP reset helper |
| `opt/xvc_server/` | Xilinx Virtual Cable server for the three modules |
| `opt/services/` | unit files as shipped by the OTA; `etc/systemd/system/` is what is installed |
| `var/www/html/` | the web UI, `libraries.json` (miner registry, Osprey entries only), and the controller C/C++ source the vendor serves from the docroot (`devices_controller.cpp`, `iic_function.cpp`, `config.cpp`, …) |
| `usr/lib/cgi-bin/`, `usr/sbin/` | the web UI's CGI (`qcmap_web_cgi.cgi`, `qcmap_auth.cgi`) and `webserver` |

Not copied here (but present in the image): `/opt/scripts`, which is the public
BeagleBoard `boot-scripts` repository.

## Leftover `/opt/trm` references

Osprey's firmware was built with support for TeamRedMiner (TRM), a third-party
miner that keeps its files under `/opt/trm`. TRM is not included in this kit, so
`/opt/trm` does not exist on a restored card. Osprey's own files still refer to it,
and they are left exactly as Osprey shipped them:

| file | what it references |
|---|---|
| `opt/controller/controller`, `controller_origin` | `/opt/trm/app.js`, `/opt/trm/algo.txt`, `/opt/trm/emailconfig.json`, and status strings such as `"Running TRM bitstream"` and `"removed trm by whitefire"` |
| `usr/sbin/webserver` | writes the selected algorithm to `/opt/trm/algo.txt` |
| `opt/updatemac/updatemac` | copies `webserver` to `/opt/trm/webserver` at boot |
| `var/www/html/devices_controller.cpp` | the controller source calls `sudo nodejs /opt/trm/app.js` (lines 2283 and 2294) |

These are compiled vendor binaries, plus the matching vendor source, and they
can't be changed without rebuilding Osprey's software. On a restored card, any of
those code paths that runs will simply find `/opt/trm` missing. None of the installed
systemd units in `etc/systemd/system/` runs anything from `/opt/trm`. The two
units that did (`webserver.service` and `frontail.service`) have been removed from
both the image and these copies.

See [`../docs/E335-overview.md`](../docs/E335-overview.md) for what each part does.
