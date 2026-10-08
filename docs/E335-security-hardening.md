# Osprey E335 — closing the box off from Osprey's back end

Osprey is defunct, but its E335 software still runs on every unit. Out of the box, an
E335 will:

* **update itself from GitHub.** Two updaters git-pull Osprey's firmware and miner
  packages every few minutes and can replace the controller, miners and loaders and
  restart them. The new code may not even run your miner, and nobody you can reach
  controls those repositories any more.
* **phone home for fleet management** to `center.ospreyelectronics.io`.
* **publish its SSH and web UI to the internet through remote.it**, which tunnels
  through your router's NAT. Port forwarding is not needed and your firewall's inbound
  rules don't apply.

If any of those back ends came back, or someone else took over the domain or the
GitHub account, they could reach every box still running the stock services. This
guide shuts all three off without touching mining, voltage control or JTAG.

Measured on firmware **N2.0.30** / algorithms **A2.2.12**.

## Quick start

```sh
git clone https://github.com/Dracnea/OspreyTechSupport.git
cd OspreyTechSupport/tools
scp e335-lockdown.sh ubuntu@<box-ip>:/tmp/
ssh -t ubuntu@<box-ip> sudo bash /tmp/e335-lockdown.sh --check   # audit only, changes nothing
ssh -t ubuntu@<box-ip> sudo bash /tmp/e335-lockdown.sh           # lock it down
```

The password is the stock `temppwd` unless you've changed it (you should, see below).
The script is safe to run again, writes a log to `/var/log/e335-lockdown.log` on the
box, and ends with a report. A locked-down box shows every vendor service as
`masked / inactive`, the binaries as `----------`, no vendor processes, and
`controller` and `xvc_server_*` still `active`.

To put everything back: `sudo bash /tmp/e335-lockdown.sh --undo`.

## What phones home, and what the script does about it

| Service | What it does | Talks to | Script action |
|---|---|---|---|
| `firmware_update` (`/opt/firmware`) | OTA updater for the control-board software. Local API on `127.0.0.1:8500`. | `github.com/PachiraMining/E300_firmware_new.git` | OTA flag off (API **and** `ota_status.txt` = 0), stop, disable, mask, binary `chmod 000` |
| `algorithm_update` (`/opt/algorithm`) | OTA updater for the miner/loader packages. Local API on `127.0.0.1:8555`. | `github.com/PachiraMining/Algorithm-official-release-v2.git` | same |
| `client_handle` (`/opt/client_handle`) | Osprey "center management" agent | `center.ospreyelectronics.io` | stop, disable, mask, `chmod 000` |
| `connectd`, `connectd_schannel` | remote.it agents. Three registered services: SSH 22, web 80 and a third management port. | `*.remot3.it`, `*.prod.yoics.org`, `api.weaved.com` | stop, disable, mask, `chmod 000` |
| `apt-daily.timer` | apt list refresh. The image has no unattended-upgrades, so nothing gets installed, but it still calls out. | `ports.ubuntu.com`, `repos.rcn-ee.com` | disabled |

The script also pins **`github.com`**, **`center.ospreyelectronics.io`** and the other
Osprey and remote.it names to `127.0.0.1` in the box's `/etc/hosts`. Even if an agent
is revived, it can't reach its back end: the OTA updaters' `git` calls fail, and the
fleet agent and remote.it can't connect. This only affects the E335 itself. Nothing
on the box needs GitHub except the OTA updaters, and the rest of your network is
untouched. Set `HOSTS_BLOCK=0` to skip it.

**Left running on purpose:** `controller` (`:8200`, voltages, fans and temperatures),
`bridge_app`, the web UI (`apache2`, `webserver`), `xvc_server_1..3` (JTAG), and all
miners and bitstream loaders.

### The OTA switch alone is not enough

Both updaters have an OTA on/off switch, reachable from the web UI or locally:

```sh
curl -X POST -d '{"otaUpdateEnable":0}' http://127.0.0.1:8500/firmware/setOtaUpdate
curl -X POST -d '{"otaUpdateEnable":0}' http://127.0.0.1:8555/algorithm/setOtaUpdate
curl http://127.0.0.1:8555/algorithm/getOtaUpdate      # {"...","otaUpdateEnable":0}
```

`firmware_update` stores the setting in `/opt/firmware/ota_status.txt` and keeps it.
**`algorithm_update` does not.** It writes `/opt/algorithm/ota_status.txt = 0` but
ignores that file on startup and logs `algorithm update status = ENABLE` every time
it starts, so the setting is lost on the next reboot. That's why the script also masks
the services and makes the binaries non-executable. Turning the switch off and
leaving the updaters running doesn't stop algorithm updates.

### Doing it by hand

The same lockdown without the script, as root on the box:

```sh
# 1. Disable, then mask, the updaters and the fleet agent, so nothing can re-enable them.
#    systemd refuses to mask a unit whose file sits in /etc/systemd/system, so move
#    those files aside first.
mkdir -p /etc/systemd/system/osprey-disabled
for u in firmware_update algorithm_update client_handle connectd connectd_schannel; do
  systemctl stop $u; systemctl disable $u
  [ -f /etc/systemd/system/$u.service ] && mv /etc/systemd/system/$u.service /etc/systemd/system/osprey-disabled/
  systemctl mask $u
done
systemctl daemon-reload

# 2. OTA flags off on disk.
echo 0 > /opt/firmware/ota_status.txt
echo 0 > /opt/algorithm/ota_status.txt

# 3. Pin the back ends to localhost.
cat >> /etc/hosts <<'HOSTS'
127.0.0.1 github.com
127.0.0.1 www.github.com
127.0.0.1 codeload.github.com
127.0.0.1 center.ospreyelectronics.io
HOSTS

# Check
systemctl is-enabled firmware_update algorithm_update client_handle   # masked x3
cat /opt/firmware/ota_status.txt /opt/algorithm/ota_status.txt         # 0, 0
getent hosts github.com center.ospreyelectronics.io                   # 127.0.0.1
```

The script does all of this plus the rest of the table, keeps backups for `--undo`,
and is safe to re-run.

## Router blocklist

This is defence in depth for boxes you haven't locked down yet, or that get reflashed
from a stock image. These are the hosts the Osprey software uses, with the addresses
they resolved to and were seen contacting in October 2026:

| Purpose | Name | Addresses seen | Where |
|---|---|---|---|
| OTA updates (both updaters) | `github.com` (the repos are under `PachiraMining/`) | 140.82.112.3, 140.82.112.4 | GitHub, US |
| Fleet management | `center.ospreyelectronics.io` | 184.169.245.74 | AWS us-west-1, US |
| Hostnames compiled into the Osprey binaries | `vptr.com`, `dracaena.io` | 148.135.17.10; 185.230.63.107, .171, .186 | US |
| remote.it tunnel relays (by far the most traffic) | `chat18.prod.yoics.org`, `chat19…`, `chat20…` | 44.236.76.190, 44.239.243.92, 44.240.35.27 | AWS us-west-2, US |
| remote.it front ends | `fe1`–`fe4.remot3.it`, `mrtg.remot3.it` | 52.38.107.102, 54.218.6.237 | AWS us-west-2, US |
| remote.it API | `api.remot3.it`, `remote.it`, `www.remote.it`, `apilb.yoics.net`, `api.weaved.com` | CloudFront (changes) | US |
| apt and NTP | `ports.ubuntu.com`, `repos.rcn-ee.com`, `ntp.ubuntu.com` | 91.189.91.157, 185.125.190.56–58 | Canonical, US/UK |

Every address an audited unit contacted was in the US or UK. Nothing went to Asia-Pacific.
To audit your own unit, run `--check`. It lists the public IPs in the box's syslog,
where its SSH logins came from, and 60 s of live outbound connections
(`SAMPLE=<seconds>` changes the length).

The remote.it and GitHub addresses are on cloud providers and change, so **block by
name, not by IP.** Blocking all of GitHub for the E335s is fine: the box only uses it
for OTA.

### Blocking them without blocking AWS or GitHub for everyone else

Everything Osprey's software talks to runs on shared infrastructure: GitHub, AWS
EC2 and CloudFront. The same IPs serve everyone else's traffic, so a router rule that
drops those addresses for the **whole network** would also break GitHub and half of
AWS for your PCs and servers. Two things avoid that:

1. **Scope every rule to the E335s.** Put the boxes in their own address list (`e335`
   below) and match `src-address-list=e335` in every drop rule. Then dropping a
   GitHub or CloudFront IP only stops the E335s from reaching it. Every other device on
   the network is untouched, whatever the address resolves to today.
2. **Better still, allow-list instead of block-list.** An E335 only ever needs DNS
   and NTP, plus your mining pools if a miner runs on the box. Allow exactly those
   and drop the rest of its WAN traffic. Then you never have to chase new Osprey,
   remote.it or GitHub addresses, and a back end that reappears under a new name
   is blocked too.

What doesn't work: blocking a single GitHub organisation or repo path. The updaters
use HTTPS, so the router sees only the hostname `github.com` (the TLS SNI), never
the `/PachiraMining/...` path. RouterOS's `tls-host` matcher can't tell one repo from
another. Scoping by source address, as above, is the reliable way. Name-based
blocking (`/ip dns static ... NXDOMAIN`) is safe network-wide only for names nobody
else uses (the Osprey, remote.it and yoics names), **never for `github.com`**.

Allow-list example (RouterOS 7; pools are matched by name, which RouterOS
re-resolves as their DNS changes):

```
/ip firewall address-list
add list=e335 address=<box-ip>                              comment="one line per E335"
add list=e335-pools address=<your-pool-host>                comment="one line per pool"

/ip firewall filter print
# Every line carries its full path, so it works whatever menu the terminal is in.
# The catch-all drop goes to the top first. place-before=0 means "before rule 0 of
# the last print", which is why the print above is needed. Each of the other four is
# then placed directly before that drop, so they end up in the order written:
#   1-2 DNS only to the router   3 allow NTP   4 allow pools   5 drop the rest to WAN
/ip firewall filter add chain=forward src-address-list=e335 out-interface-list=WAN action=drop place-before=0 comment="e335: nothing else"
/ip firewall filter add chain=forward src-address-list=e335 protocol=udp dst-port=53 action=drop place-before=[/ip firewall filter find comment="e335: nothing else"] comment="e335: router DNS only"
/ip firewall filter add chain=forward src-address-list=e335 protocol=tcp dst-port=53 action=drop place-before=[/ip firewall filter find comment="e335: nothing else"] comment="e335: router DNS only"
/ip firewall filter add chain=forward src-address-list=e335 protocol=udp dst-port=123 action=accept place-before=[/ip firewall filter find comment="e335: nothing else"] comment="e335: NTP"
/ip firewall filter add chain=forward src-address-list=e335 dst-address-list=e335-pools protocol=tcp action=accept place-before=[/ip firewall filter find comment="e335: nothing else"] comment="e335: pools"
```

Do not rely on "add them in reverse order, each with place-before=0": in a pasted
block every `0` refers to the same rule from the last `print`, so the rules land in
the order they were typed, with the catch-all drop on top, and every pool connection
is dropped. To start over:
`/ip firewall filter remove [find comment~"^e335: "]`, then paste the block again.

Check the order with `/ip firewall filter print stats where comment~"e335"`: "nothing else" must
be last, and once a box has talked to its pool the "pools" counter is above zero. They must come
before any rule that accepts **new** LAN-to-WAN connections; an "accept established,
related" rule above them is fine. The DNS drops only affect DNS servers *outside*
the router: queries to the router itself go through the `input` chain. LAN traffic
(your miner PC to the box's XVC and controller ports) never leaves through the WAN
and keeps working.

### MikroTik (RouterOS 6.36+ / 7)

RouterOS resolves domain names in address lists and keeps them updated:

```
/ip firewall address-list
add list=e335 address=<box-ip>      comment="one line per E335"
add list=osprey-block address=center.ospreyelectronics.io
add list=osprey-block address=vptr.com
add list=osprey-block address=dracaena.io
add list=osprey-block address=github.com
add list=osprey-block address=remote.it
add list=osprey-block address=www.remote.it
add list=osprey-block address=api.remot3.it
add list=osprey-block address=fe1.remot3.it
add list=osprey-block address=fe2.remot3.it
add list=osprey-block address=fe3.remot3.it
add list=osprey-block address=fe4.remot3.it
add list=osprey-block address=mrtg.remot3.it
add list=osprey-block address=chat18.prod.yoics.org
add list=osprey-block address=chat19.prod.yoics.org
add list=osprey-block address=chat20.prod.yoics.org
add list=osprey-block address=apilb.yoics.net
add list=osprey-block address=api.weaved.com

/ip firewall filter
add chain=forward src-address-list=e335 dst-address-list=osprey-block action=drop \
    comment="E335: no Osprey / remote.it / OTA" place-before=0
```

Optionally, make the names fail to resolve as well (RouterOS 7, if the boxes use the
router for DNS):

```
/ip dns static
add name=center.ospreyelectronics.io type=NXDOMAIN
add regexp=".*\\.remot3\\.it\$" type=NXDOMAIN
add regexp=".*\\.yoics\\.(net|org)\$" type=NXDOMAIN
```

**Stricter option:** if your miner runs on a PC and drives the E335 over the LAN (XVC,
controller API), the box doesn't need the internet at all. Drop everything from the
`e335` list to the WAN and allow only NTP if you want the clock right:

```
/ip firewall filter
/ip firewall filter print
/ip firewall filter add chain=forward src-address-list=e335 out-interface-list=WAN action=drop place-before=0 comment="e335: nothing else"
/ip firewall filter add chain=forward src-address-list=e335 protocol=udp dst-port=123 action=accept place-before=[/ip firewall filter find comment="e335: nothing else"] comment="e335: NTP"
```

If a miner runs on the box itself, allow its pool's host and port before the drop rule.

## Also worth doing

* **Change the stock password.** Every E335 ships with `ubuntu` / `temppwd`, and the
  same password gives `sudo`. Run `passwd` on the box.
* **Keep `:8200`, `:2541–2543` and `:80` on the LAN.** The controller API (which sets
  voltages), the XVC JTAG servers and the web UI have no authentication worth the name.
  Never port-forward them.
* **remote.it registrations.** The stock units carry device registrations in
  `/etc/connectd/services/*.conf`. The script leaves them in place so `--undo` works.
  Once the agents are masked, they're inert. If you will never use remote.it, you can
  delete the three `.conf` files. The SD image in [`../firmware/`](../firmware/) already
  ships without them.

## Undo

`sudo bash e335-lockdown.sh --undo` restores the original unit files (kept in
`/etc/systemd/system/osprey-disabled/`) and binary modes (in `modes.txt` there),
re-enables and starts the services and timers, and removes the `/etc/hosts` block. The
firmware OTA flag stays off. Turn it on with the `curl` call above, using `1`.
