# Which Raspberry Pi software runs on the TI TMDS62LEVM (AM62L), and what does it cost?

> Disclosure: Fox Red Labs is a lab run by a TI employee on personal time. Contributors' projects are their own. Not affiliated with, sponsored by, or endorsed by Texas Instruments or any other manufacturer.

Mosquitto, Node-RED, Pi-hole and Home Assistant Container all run on the board's two Cortex-A53 cores from TI's own SDK Linux image, because that image already carries Docker. Python GPIO (gpiozero and Adafruit Blinka) toggles the user LED after three workarounds. No Debian or Armbian image is needed. Checked 2026-10-24 on one TMDS62LEVM: TI Linux SDK 12.01 (Arago 2026.04, kernel 6.18.38) booted from a 119 GB micro-SD card, Docker 29.3.0, both cores at 1.25 GHz (`performance` governor), 1,960 MB of RAM, Ethernet to a home network. 1 kB = 1,024 bytes. Terms: [docs/vocab.md](../../docs/vocab.md). Board basics: [../README.md](../README.md).

## What is in the image

| Tool | Version | Note |
|---|---|---|
| Docker, containerd | 29.3.0, overlayfs storage, cgroup v2 | running at boot; the kernel has overlayfs, bridge, veth, nf_tables, iptables NAT, conntrack and memory cgroups |
| Python, pip | 3.14.5, pip 26.0.1 | gcc, make and git are present; no swig |
| Node.js | 22.22.3 | |
| libgpiod tools | 2.2.4 (`gpiodetect`, `gpioinfo`, `gpioset`, `gpioget`) | |
| Mosquitto | 2.1.2 as a service on 127.0.0.1:1883 | holds the port, see the traps |
| opkg | 0.9.0 | its package feed host does not resolve outside TI's network, so `opkg install` cannot fetch anything |
| apt | none | |

Source: the commands in this page, run 2026-10-24.

## Results

Every row is in `data/results/am62l-evm.csv` (tests `linux_app_*` and `rail_power_mw`, variants named below). One run each unless stated. "System used" is MemTotal minus MemAvailable from `/proc/meminfo`; it moves by about 50 MB with the page cache, so the container figure from `docker stats` is the cleaner one. Idle is the share of CPU time both cores spent idle over 30 s once the app was ready. Power is the sum of the eight INA228 rails the board carries (`am62l-evm/tools/pwr.py`, 30 s mean).

| App (image) | Pull | Image on disk | Ready after `docker run` | Container RAM | System used | CPU idle | Rails |
|---|---|---|---|---|---|---|---|
| none (Docker running, native Mosquitto, TI's LVGL demo) | | | | | 521 MB | 93.0% | 567.6 mW |
| Mosquitto 2 (`eclipse-mosquitto:2`) | 9.1 s | 43 MB | 2.4 s to port 1883 | 2.6 MiB | 598 MB | 92.6% | 569.1 mW |
| Node-RED 5.0.9 (`nodered/node-red:latest`) | 197 s | 992 MB | 17.7 s to the editor | 110 MiB | 639 MB | 88.9% | 571.8 mW |
| Pi-hole 2026.10.0, FTL v6.8 (`pihole/pihole:latest`) | 33 s | 166 MB | 4.6 s to the first DNS answer | 24.8 MiB | 576 MB | 91.0% | 572.4 mW |
| Home Assistant Container 2026.10.4 (`ghcr.io/home-assistant/home-assistant:stable`) | 655 s | 3.42 GB | 67.7 s to the onboarding API (two starts, both 67.7 s) | 557 MiB | 1,079 MB | 91.5% | 571.5 mW |
| Home Assistant plus Mosquitto container, MQTT sensor added | | | | 559 MiB plus 4.2 MiB | 1,131 MB | 82.5% (20 s after setup, not steady) | 569.5 mW |

Reading the table:

- Power does not move. Every state is within 5 mW (0.9%) of the 567.6 mW idle figure, because the A53 cores sit idle most of the time. These are idle-service figures, not a load test.
- Home Assistant is the only app that takes a large share of RAM: about 480 MB more system memory than idle, a quarter of the 1,960 MB. The other three fit in 25 to 110 MB.
- The 75 MB "system used" step for Mosquitto against 2.6 MiB in the container is most likely Docker's own cost (daemon, shim, mount) rather than the broker; it was not split out. TI's native Mosquitto service uses 7.7 MB resident.
- The image pulls ran over Ethernet to the SD card; Home Assistant's 655 s is dominated by unpacking onto the card.

## Proof of one real action each

| App | Action | Result |
|---|---|---|
| Mosquitto, native service | `mosquitto_sub` on one shell, `mosquitto_pub` on another | message received |
| Mosquitto, container | publish from the Mac with `mqtt_min.py` while the board subscribes in the container | `frl/mac hello-from-mac` received on the Mac, and `hello-from-board` received in the container |
| Node-RED | `POST /flows` with an http-in, function and http-response flow ([flow](#node-red)), then `GET /frl-hello` | `{"hello":"from node-red on am62l","n":42,...}` 2.5 s after the deploy |
| Pi-hole | `dig @<board address> doubleclick.net`, `googleadservices.com`, `ads.google.com`, `example.com` | the first three answer `0.0.0.0`, `example.com` answers two real addresses; gravity held 71,493 domains |
| Home Assistant | `ha_bootstrap.py`: onboarding user, MQTT integration to the Mosquitto container, one discovery message and one state message | `sensor.frl_bench_frl_test_sensor` reads `21.5` through `/api/states` 29 s after the publish |

## Commands

Everything below runs as root on the board over ssh (`<board address>` is the DHCP address on `eth1`; read it from the console with `am62l-evm/tools/sh.py "ip -4 -o addr show eth1"`).

### Mosquitto

TI's service already holds 127.0.0.1:1883, so stop it before the container (`systemctl stop mosquitto`) or publish the container on another port.

```
docker run -d --name mosq -p 1883:1883 -v /root/mosquitto.conf:/mosquitto/config/mosquitto.conf:ro eclipse-mosquitto:2
docker exec mosq mosquitto_sub -h 127.0.0.1 -t frl/test -C 1 -W 10 &
docker exec mosq mosquitto_pub -h 127.0.0.1 -t frl/test -m hello
python3 am62l-evm/linux-apps/mqtt_min.py pub <board address> 1883 frl/mac hello
```

`mosquitto.conf` is `listener 1883` and `allow_anonymous true`, which is only for a bench on a private network.

### Node-RED

```
docker run -d --name nr -p 1880:1880 nodered/node-red:latest
curl -X POST -H "Content-Type: application/json" -H "Node-RED-Deployment-Type: full" --data @flow.json http://127.0.0.1:1880/flows
curl http://127.0.0.1:1880/frl-hello
```

`flow.json` is three nodes: `http in` (GET `/frl-hello`), a `function` node (`msg.payload = {hello: 'from node-red on am62l', n: 6*7, t: Date.now()}`) and `http response`.

### Pi-hole

systemd-resolved listens on 127.0.0.53 and 127.0.0.54 only, so publish port 53 on the board's own address, not on all addresses.

```
docker run -d --name ph -p <board address>:53:53/tcp -p <board address>:53:53/udp -p 8080:80 \
  -e TZ=UTC -e FTLCONF_webserver_api_password=<password> -e FTLCONF_dns_listeningMode=all \
  -e FTLCONF_dns_upstreams=1.1.1.1 pihole/pihole:latest
dig @<board address> doubleclick.net +short
```

The blocklist loads a few minutes after the first answer; until it finishes, blocked names resolve normally.

### Home Assistant Container

```
mkdir -p /root/ha-config
docker run -d --name ha --network host -e TZ=UTC -v /root/ha-config:/config ghcr.io/home-assistant/home-assistant:stable
HA_BENCH_PASSWORD=<password> python3 ha_bootstrap.py http://127.0.0.1:8123 127.0.0.1 1883 127.0.0.1 1883
```

`ha_bootstrap.py` creates the first user through the onboarding API (31.9 s on this board, the password hash), finishes onboarding, adds the MQTT integration, publishes a discovery message and reads the sensor back. Host networking lets Home Assistant reach the broker on 127.0.0.1. Home Assistant logs a Bluetooth permission error; the container needs `NET_ADMIN` and `NET_RAW` for that and the test does not use it.

## Python GPIO

LD8 is GPIO0_123 and SW5 is GPIO0_90, both on `gpiochip0` (`600000.gpio`, 126 lines). The kernel's `leds-gpio` (heartbeat trigger) and `gpio-keys` drivers own them, so free them first and give them back afterward:

```
sh gpio_claim.sh free        # stops the heartbeat, unbinds the drivers
sh gpio_claim.sh restore
```

Install (a few minutes; `pip` needs `--break-system-packages` on this image):

```
pip3 install --break-system-packages setuptools wheel "swig==4.3.1" gpiozero gpiod adafruit-blinka
curl -L https://github.com/joan2937/lg/archive/refs/heads/master.tar.gz | tar xz
cd lg-master && make && make install        # liblgpio and its Python module, into /usr/local and site-packages
export LD_LIBRARY_PATH=/usr/local/lib
```

| Library | Result | Check |
|---|---|---|
| gpiod tools | `gpiodetect` lists gpiochip0 to gpiochip4; `gpioinfo` shows line 123 `output consumer="am62-sk:green:heartbeat"` and line 90 `input active-low consumer="User Key"` before freeing, both unnamed and free after | `gpioinfo -c gpiochip0` |
| gpiozero 2.0.1 with the lgpio factory | `gpiozero_demo.py blink 3` toggles LD8 at 2 Hz | OUT_DATA bit 27 of GPIO0 bank 3 read through `/dev/mem` (`gpio_regs.py`) went 0, 1, 0, 1 with the script |
| Adafruit Blinka 9.2.0 | `blinka_demo.py 3` toggles LD8; `board.board_id` reads `AM62L_EVM` | same register check |
| SW5 read | idle read: gpiozero `is_pressed` False, Blinka `value` True, IN_DATA bit 26 of bank 2 = 1 | a press was not recorded in this run |

The check is the register, not the line value: reading LD8 back always gives 0, which fits a pad with its input buffer off (the device tree sets `PIN_OUTPUT`; not checked in the pad register), and `led.toggle()` or `led.value = not led.value` never changes state. The scripts keep the state themselves. The LED was not watched by eye or camera.

Blinka's own AM625X board file (the BeaglePlay, `adafruit_blinka/microcontroller/am625x/pin.py` and `board/beagleboard/beagle_play.py`, MIT, Adafruit Industries 2026) was the starting point. `blinka_am62l.py` writes an AM62L chip module and an `AM62L_EVM` board module (`blinka/pin.py`, `blinka/am62l_evm.py`) into the installed package, adds both to Blinka's import tables, and adds device-tree detection (`ti,am62l3`, `ti,am62l3-evm`) to Adafruit-PlatformDetect 3.89.1. The board file lists only LD8 and SW5; no I2C, SPI or UART port is mapped, and the EVM's expansion headers were not traced. Without the detection patch, `BLINKA_FORCECHIP=AM62L BLINKA_FORCEBOARD=AM62L_EVM` selects the board.

gpiozero 2.0 only knows Raspberry Pi board revisions, so `gpiozero_demo.py` subclasses `LGPIOFactory` and hands it a board description of the two lines. Pin numbers are the line offsets on chip 0.

## If you see this

- If `docker run -p 1883:1883` says `address already in use`: TI's Mosquitto service holds the port. `systemctl stop mosquitto` first, `systemctl start mosquitto` after.
- If `opkg update` prints `Downloading` lines and then `Could not resolve host` on a manual fetch: the feed host in `/etc/opkg/base-feeds.conf` is not reachable outside TI's network. Use Docker or pip.
- If `pip install lgpio` fails with `swig: No such file or directory`: `pip install swig`. If it then fails with `PyInt_FromLong` errors, install `swig==4.3.1` (4.5.1 generates code that does not compile against Python 3.14 here) and use `--no-build-isolation`. If it ends with `cannot find -llgpio`, build the C library as shown above.
- If gpiozero raises `PinUnknownPi: unable to locate Pi revision`: it needs a board description; use `gpiozero_demo.py`'s factory.
- If a gpiod or lgpio request fails with `Device or resource busy`: run `gpio_claim.sh free`.
- If the Home Assistant MQTT step answers 400 `required key not provided at 'other_settings.set_client_cert'`: the Home Assistant 2026.10 broker form needs `other_settings` with `set_client_cert` false and `set_ca_cert` `off`, as `ha_bootstrap.py` sends.
- If Home Assistant's page does not appear for a minute or more after `docker run`: that is normal here; the onboarding API answered after 67.7 s both times.

## Files

| File | What it is |
|---|---|
| `mosquitto.conf` | Broker config for the container, bench use only |
| `mqtt_min.py` | MQTT publish and subscribe with no dependencies (works from a Mac) |
| `ha_bootstrap.py` | Home Assistant onboarding by API, MQTT entry, one discovery sensor, state read-back; the bench password comes from `HA_BENCH_PASSWORD` or is generated and printed once |
| `sample.py` | Memory and CPU idle over N seconds, one line |
| `gpio_claim.sh` | Free or restore LD8 and SW5 |
| `gpio_regs.py` | Read LD8's output register and SW5's input register without touching the lines |
| `gpiozero_demo.py` | LD8 blink and SW5 read with gpiozero and lgpio |
| `blinka_am62l.py`, `blinka/` | Blinka and PlatformDetect patch for the AM62L EVM |
| `blinka_demo.py` | LD8 blink and SW5 read with Blinka |

Evidence: `docs/experiments/2026-10-24-am62l-linux-apps.md`, `data/results/am62l-evm.csv`.
