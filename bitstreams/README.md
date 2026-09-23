# Osprey E335 bitstreams

Osprey-developed bitstreams for the E335, recovered from units running firmware
N2.0.30 / A2.2.12. Each `.bit` is stored as a `.bit.tar.xz` holding one file with
no directory prefix; the matching `.md5sum` is the checksum of the **uncompressed**
`.bit`, in the format the vendor loader expects.

| file | miner | Vivado header part | built | md5 (uncompressed) |
|---|---|---|---|---|
| `astrix/e335_v1.bit` | `/opt/astrix` | `xcvu35p-fsvh2104-2LV-e_CIV-fsvh2104-2L-e` | 2024-11-06 | `cc6c59599b73be0e866ad95d559f231c` |
| `hoohash/e335_v1.bit` | `/opt/hoohash` | `xcvu35p-fsvh2104-2LV-e_CIV-fsvh2104-2-e` | 2024-12-13 | `e2dd4af0ece80d9b73ae26199df133c5` |
| `tari_os/e335_v1.bit` | `/opt/tari_os` | `xcvu35p-fsvh2104-2L-e` | 2025-05-21 | `1f713c735830bdddf06cd565a0b24361` |
| `tari_os/e335_v2.bit` | `/opt/tari_os` | `xcvu35p-fsvh2104-2LV-e` | 2025-06-04 | `2e37ac29d9bebef9cdd4eccfc1f60cdc` |
| `tari_os/e335_v3.bit` | `/opt/tari_os` | `xcvu35p-fsvh2104-2LV-e` | 2025-06-18 | `ee9236dfc012ca0a8d4a5132d56ce20a` |

All are `miner_top`, Vivado 2022.1, roughly 56.7 MB uncompressed. Every file was
pulled individually, checked against a checksum taken on the unit itself, and
re-verified after compression.

The Astrix and Hoohash part strings are two part names run together — a quirk of
how they were built, not a real part. The leading `xcvu35p` is consistent with
the hardware (IDCODE `0x14b71093`, plain VU35P).

## Installing one

```sh
tar -xJf tari_os/e335_v3.bit.tar.xz                      # -> e335_v3.bit
scp e335_v3.bit tari_os/e335_v3.bit.md5sum ubuntu@<box-ip>:/tmp/
ssh ubuntu@<box-ip>
sudo cp /tmp/e335_v3.bit /tmp/e335_v3.bit.md5sum /opt/tari_os/bits/
cd /opt/tari_os/bits && md5sum -c e335_v3.bit.md5sum    # must say OK
```

**Keep the `.md5sum` next to the `.bit`**: the vendor loader verifies it and
**deletes the bitstream** if it is missing or does not match. If the unit's SD
card is suspect, stage into a tmpfs instead (see
[`../docs/E335-overview.md`](../docs/E335-overview.md#loading-bitstreams)).
