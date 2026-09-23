# Osprey E335 SD card image — firmware N2.0.30 / algorithms A2.2.12

A complete, bootable E335 SD card: the boot partition byte-for-byte and the root
filesystem as a tar. Captured from a working unit running the latest
Osprey release we have seen (`/opt/firmware/firmware_version.txt` = `N2.0.30`,
`/opt/algorithm/algorithm_version.txt` = `A2.2.12`), then cleaned for publication.

To write a card, use [`../tools/flash-e335-sd.sh`](../tools/flash-e335-sd.sh) and
read [`../docs/E335-sd-card-recovery.md`](../docs/E335-sd-card-recovery.md).

| file | what it is |
|---|---|
| `boot/e335-boot-p1.img.xz` | raw image of `mmcblk0p1`, the 512 MiB FAT32 boot partition: `BOOT.BIN` (the Zynq boot image, including U-Boot), the U-Boot boot script, the kernel and the device tree. **This is the low-level firmware.** |
| `rootfs/e335-rootfs.tar.xz.part-00..02` | the root filesystem (`mmcblk0p2`) as one `.tar.xz`, split into parts under GitHub's 100 MB file limit. Reassemble with `cat e335-rootfs.tar.xz.part-* > e335-rootfs.tar.xz`. |
| `SHA256SUMS` | checksums for all of the above — `sha256sum -c SHA256SUMS` |

## Stock partition layout

```
/dev/mmcblk0p1   start 8192      1048576 sectors   512M  type b   W95 FAT32   label BOOT
/dev/mmcblk0p2   start 1056768   10483593 sectors    5G  type 83  Linux       label root
```

The flash script keeps both start offsets and makes p2 8 GiB by default; grow it
to fill the card afterwards if you like.

## Manual restore (what the script automates)

```sh
cat rootfs/e335-rootfs.tar.xz.part-* | xz -dc > /var/tmp/e335-rootfs.tar
# partition /dev/sdX with the layout above, then:
xz -dc boot/e335-boot-p1.img.xz | sudo dd of=/dev/sdX1 bs=1M conv=fsync status=progress
sudo mkfs.ext4 -L root /dev/sdX2
sudo mount /dev/sdX2 /mnt
sudo tar xf /var/tmp/e335-rootfs.tar -C /mnt --numeric-owner -p
# give it an identity -- the image has NO ssh host keys, so sshd won't start without this:
sudo ssh-keygen -A -f /mnt
sudo dd if=/dev/zero of=/mnt/swapfile bs=1M count=512 && sudo chmod 600 /mnt/swapfile && sudo mkswap /mnt/swapfile
sudo umount /mnt
```

## What was changed from the unit it was captured from

The image is the vendor system with the following removed or reset, so that it
is safe to share and behaves like a fresh unit:

**Removed**

* All miners and bitstreams **not developed by Osprey**, with their systemd units
  and web-UI registry entries (`libraries.json` now lists only the Osprey group).
* The 2 GiB swapfile (the flash script recreates a smaller one).
* SSH host keys (regenerated per card by the flash script), shell histories, npm
  caches, apt package lists and caches (run `sudo apt-get update` if you need
  apt), DHCP leases, connman's remembered networks.
* A custom bring-up service that had been added on the source unit.
* The device registration of the pre-installed **remote.it** remote-access agent
  (`/etc/connectd/services/*.conf`, registration key, hardware and bulk IDs).
  The agent's binaries are still installed but it is not registered to anyone.
  Units in the field may still carry a registration — if you don't use remote.it,
  check `/etc/connectd/services/` on yours.

**Reset**

* `/etc/network/interfaces` and `interfaces_backup` → plain DHCP, no pinned MAC.
* `/etc/hostname` → `arm` (the stock name). `/etc/machine-id` → empty.
* Miner start scripts (`startastrix.sh`, `starttari_os.sh`) → the placeholder
  pool / wallet / worker form of Osprey's own `sample.sh`.
* `/opt/controller/config/config.json` → factory rails (VCCINT 645 mV, VCC_HBM
  1100 mV), `temps_max` 90 °C, automatic fan mode.
* All logs under `/var/log` and `/opt/bridge_app/log` emptied; web-UI session
  files emptied.
* `tari_os` no longer starts at boot; pick and configure a miner in the web UI.

**Not changed**

* The web-UI login and the `ubuntu` account keep their vendor defaults (SSH:
  `ubuntu` / `temppwd`). **Change them** after first boot.
* Osprey's own binaries are byte-for-byte as shipped. Some of them (for example
  the controller) contain built-in references to other vendors' miners and
  default pool addresses; those are compiled into Osprey's code and were left
  as-is.

## Bitstreams

The `/opt/<algo>/bits/` directories in this image are **empty** — the unit it was
captured from had them removed. Osprey's E335 bitstreams are in
[`../bitstreams/`](../bitstreams/); copy the right one into place before starting
a miner.
