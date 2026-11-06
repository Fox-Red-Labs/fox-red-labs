# The portable app as plain C on the TMDS62LEVM (AM62L) under Linux

Terms: [docs/vocab.md](../../../docs/vocab.md). `common/app.c` builds unchanged as a Linux program. `glue_linux.c` (66 code lines) implements the five glue calls with
plain sysfs files and no libraries; `config_am62l.h` (5 code lines) names the two files on the TMDS62LEVM.

| Call | Linux way on the TMDS62LEVM |
|---|---|
| `adc_read` | `/sys/bus/iio/devices/iio:device0/in_voltage0_raw` (TI-am335x-adc, 12 bit, 0.439 mV per count) |
| `uart_write` | the tty in `FRL_UART` (here `/dev/ttyS0`, UART0, the console on FT4232 port A), else stdout |
| `led_toggle` | `/sys/class/leds/am62-sk:green:heartbeat/brightness`, after setting its trigger to `none` |
| `sleep_ms` | `nanosleep` |

Run, on the board (Debian trixie arm64 container for the compiler, since the image has none):

```sh
sh linux/build.sh /root/frl/tw                     # copy common/ and linux/ to /root/frl/tw first
FRL_UART=/dev/ttyS0 timeout 14 /root/frl/tw/linux/build/three_ways_static
stty -F /dev/ttyS0 opost                           # give the console its output processing back
echo heartbeat > /sys/class/leds/am62-sk:green:heartbeat/trigger
```

On the Mac: `python3 serial_check.py /dev/cu.usbserial-<id>0 16 /tmp/run-linux-am62l.txt` prints PASS
(14 lines, gaps 0.991 to 1.008 s, 2026-10-25). Numbers: `data/results/am62l-evm.csv`, variants `linux-sysfs` and
`three-ways-app-linux-*`.

If you see each line end in `\r\r\n` and the checker report 2 s gaps, the tty's output processing turned `\n` into
`\r\n`: the glue clears `OPOST`, so restore it with `stty -F /dev/ttyS0 opost` after the run.

## The same glue on the BeagleY-AI (AM67A)

`glue_linux.c` and `common/app.c` build unchanged with the BeagleY-AI's own gcc. The board's two paths go in `boards/beagley-ai/config_beagley_ai.h` (2 code lines: the LED is `/sys/class/leds/ACT`; the board has no ADC, so the ADC path does not exist and every line reads `adc=65535`), passed in place of `config_am62l.h`:

```sh
gcc -std=c99 -D_POSIX_C_SOURCE=200809L -O2 -Icommon -Ilinux -DCONFIG_AM62L_H -include linux/boards/beagley-ai/config_beagley_ai.h common/app.c linux/glue_linux.c -o three_ways
timeout 14 ./three_ways
```

Result 2026-11-03: built in 2.3 s, 66.1 kB stripped, 14 lines at one a second (`beagley-ai/bench/portability/three-ways-run.log`; matrix id X56). The image has no `libc.a`, so there is no static build.
