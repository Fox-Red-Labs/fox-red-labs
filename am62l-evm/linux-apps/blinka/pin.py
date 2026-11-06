# SPDX-License-Identifier: MIT
"""TI AM62L pin names (TMDS62LEVM).

Modeled on adafruit_blinka/microcontroller/am625x/pin.py (BeaglePlay, MIT, Adafruit Industries, 2026):
each Pin is a libgpiod (chip, line) pair. On the AM62L EVM, gpiochip0 is 600000.gpio (GPIO0, 126 lines).
"""
from adafruit_blinka.microcontroller.generic_linux.libgpiod_pin import Pin

# gpiochip0 (600000.gpio), GPIO0
LD8 = Pin((0, 123))        # user LED LD8, GPIO0_123 (pad MMC1_SDWP, ball D24), active high
SW5 = Pin((0, 90))         # user button SW5, GPIO0_90 (pad SPI0_D0, ball E12), reads 0 while pressed

# No I2C, SPI or UART port is listed: the EVM's 40-pin-style headers were not mapped in this file.
i2cPorts = ()
spiPorts = ()
uartPorts = ()
