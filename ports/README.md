# Ports: other ecosystems' code running on TI LaunchPad kits

Terms: [docs/vocab.md](../docs/vocab.md). Each folder takes code written for something else and runs it on a TI LaunchPad kit: ESPHome (`esphome-ti/`), a handwriting model (`bitnetmcu/`), a GPS parser (`minmea/`), Nordic's phone apps (`nordic-apps/`), a Bluetooth media remote (`ble-hid-remote/`), upstream OpenOCD for the CC23xx chips (`openocd-cc23xx/`), and another vendor's example pack (`cms32/`). Every folder's README says what ran, what only built and where it stopped, with the build command.

Every port with its status in one table: `docs/ports.md`. Upstream code is copied unmodified into `third-party/`, never edited here. Not here: the vendor files with no usable license, prebuilt images, and ports kept private for now. Take one folder with `git sparse-checkout add ports/<name> tools data third-party`.

| Folder | What runs | Where to start |
|---|---|---|
| `micropython/zephyr-cc2340r5/` | MicroPython 1.29.0 through upstream's Zephyr port on the LP-EM-CC2340R5: prompt, pins, ADC, `Pin.irq`, PWM, SPI, I2C | `micropython/zephyr-cc2340r5/README.md` (build with `build.sh`) |
| `renesas-ra-fsp/` | Three Renesas FSP examples (I2C master, UART with PWM, ADC) on the LP-MSPM0G3519, on a layer that maps FSP calls to TI DriverLib | `renesas-ra-fsp/README.md`; layer in `../portability/fsp-on-mspm0/`, source in `../third-party/renesas/` |
| `../portability/arduino/zephyr/` | Arduino's own sketches (Blink, AnalogReadSerial, interrupts, PWM, SPI, Wire) through the Zephyr-based Arduino core on the LP-EM-CC2340R5 and LP-MSPM0G3519 | `../portability/arduino/zephyr/README.md` |
| `../portability/zephyr-mspm0/` | Zephyr's MSPM0 I2C driver (open pull request) and a flash driver on the LP-MSPM0G3519, with the upstream pull request texts | `../portability/zephyr-mspm0/README.md` |

