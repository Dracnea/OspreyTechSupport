# Osprey E335 — SD card failure, backup and recovery

The E335 boots and runs entirely from a microSD card, and **the cards in these
units wear out**. Several units we have handled had cards that were failing, and
the failure is quiet enough to cost hours if you don't recognise it. This page is
how to recognise it, how to back a unit up, and how to write a new card from the
image in [`../firmware/`](../firmware/).

## Recognising a failing card

Look in `dmesg` for any of:

```
EXT4-fs error (mmcblk0p2): ext4_find_extent: inode #...: bad header/extent: invalid magic ...
EXT4-fs (mmcblk0p2): Delayed block allocation failed ... with error 117
EXT4-fs (mmcblk0p2): This should not happen!! Data will be lost
EXT4-fs error (device mmcblk0p2): ext4_mb_generate_buddy: ... block bitmap and bg descriptor inconsistent
blk_update_request: I/O error, dev mmcblk0, sector ...
mmc0: Problem switching card into high-speed mode!
```

`mmc0: Problem switching card into high-speed mode!` appears **once at boot on
healthy units too**, so on its own it means nothing. If it **recurs while the box
is running**, it does.

Symptoms that mislead:

* **A file written and read back at the right size but full of zero bytes**, with
  no I/O error anywhere. A small shell script copied to the box came back as all
  NULs, so `bash` said `cannot execute binary file`. A bitstream load that reports
  "0 of 3 modules DONE" can be exactly this, not a bad bitstream.
* **A bitstream whose md5 does not match at the correct size.** On a healthy card
  that would mean a different image; on a failing one it is a bad read. You cannot
  tell the two apart from one reading — so never delete a file on the strength of
  a checksum taken from a suspect card.
* **The controller never starts** — no web UI, no `:8200` API, so the box cannot
  configure its FPGAs or set its rails.

### A test that tells you

```sh
# 1. do freshly written blocks survive a genuinely cold read?
dd if=/dev/urandom of=/home/ubuntu/coldtest bs=1M count=32; sync
md5sum /home/ubuntu/coldtest
sync; echo 3 | sudo tee /proc/sys/vm/drop_caches     # drop the page cache
md5sum /home/ubuntu/coldtest                         # repeat 3x; all must agree
rm /home/ubuntu/coldtest

# 2. md5 every bitstream twice, dropping caches between the passes
```

A card can pass (1) and still fail (2): fresh writes are fine while **older data
is not**. Such a unit may mine perfectly — the bitstream is loaded once and the
hot path never re-reads those sectors — but the card should be replaced at the
first opportunity.

## Backing a unit up

**Never trust a bulk archive taken off the box.** Streaming all of `/opt` through
one `tar -czf -` produced an archive with a *valid* gzip CRC in which 6 of 15
bitstreams did not match the box's own md5 of the same files. gzip's CRC only
certifies what `tar` handed it; a bad read off a failing card passes silently.
And `cat` followed immediately by `md5sum` both hit the page cache, so they agree
without proving anything about the card.

So, for anything you care about:

* copy files **one at a time** and compare each against an `md5sum` taken on the
  box in the same pass;
* for important files, force genuinely cold reads (drop caches, or read a large
  unrelated file in between) and require several reads to agree;
* better still, power the box down, put its card in a PC reader, and image it
  from there.

To capture a whole card on a PC (card in a reader as `/dev/sdX`):

```sh
sudo sfdisk -d /dev/sdX > parttable.txt
sudo dd if=/dev/sdX1 bs=1M status=progress | xz -T0 > boot-p1.img.xz
sudo mount -o ro /dev/sdX2 /mnt
sudo tar -C /mnt --numeric-owner -cpf - . | xz -T0 > rootfs.tar.xz
sudo umount /mnt
```

The root filesystem is captured as a tar rather than a block image because units
are often grown to fill a 64 GB card; a tar restores onto a card of any size.

## Writing a new card

You need: a microSD card of **16 GB or more** (the default layout uses about
9 GB and the root filesystem is about 1 GB), a card reader on a Linux PC with
`xz`, `sfdisk` and `mkfs.ext4`, and this repository.

```sh
git clone https://github.com/Dracnea/OspreyTechSupport.git
cd OspreyTechSupport/tools
lsblk                                       # find the card, e.g. /dev/sdb
sudo DEV=/dev/sdb ./flash-e335-sd.sh        # asks you to type the device name
sudo DEV=/dev/sdb ./verify-e335-sd.sh       # read-only check before booting it
```

What [`flash-e335-sd.sh`](../tools/flash-e335-sd.sh) does:

1. checks the firmware against `firmware/SHA256SUMS` and refuses to write to
   anything that looks like a system disk;
2. partitions the card with the **stock geometry** — p1 at sector 8192, 512 MiB
   FAT32 boot; p2 at sector 1056768, ext4 root (`ROOT_GIB`, default 8 GiB);
3. writes the boot partition byte-for-byte and extracts the root filesystem in
   resumable chunks (see below);
4. gives the card its **own identity**: fresh SSH host keys, an empty machine-id,
   a hostname (`HOST`, default `arm`) and a random locally-administered MAC pinned
   on eth0 (`MAC=...` to choose one, `MAC=keep` to let `/opt/updatemac` decide);
5. creates a swapfile and verifies what it wrote.

The unit comes up on **DHCP**. Find it in your router's lease table (the script
prints the MAC), then `ssh ubuntu@<address>` with password `temppwd` — and change
it. Optionally grow the root partition to fill the card afterwards:

```sh
sudo sfdisk -N 2 --force /dev/sdX <<< ", +"
sudo e2fsck -f /dev/sdX2 && sudo resize2fs /dev/sdX2
```

### If your USB card reader keeps disconnecting

Some cheap USB SD adapters electrically drop off the bus after 1.5–2 GB of
sustained writing (`usb X-Y: USB disconnect` in `dmesg`) and don't come back
without being unplugged. The script is built around this: it throttles the
kernel's write-back, pauses between chunks (`COOLDOWN`, default 30 s), and records
progress on the card itself. If the reader drops, reseat it and **run the same
command again** — it resumes at the next chunk rather than starting over.

### Why a stock hostname and a fresh MAC matter

If two units boot with the same pinned MAC they fight over one DHCP lease and the
switch flaps between them — the new box looks like it "steals" another unit's
network. Every card written from a shared image must get its own MAC, SSH keys
and machine-id, which the script does.

## What is in the image

See [`../firmware/README.md`](../firmware/README.md).
