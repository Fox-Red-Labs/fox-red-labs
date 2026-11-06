# The ten-minute path: MicroPython on the Pico 2. Save this as main.py on the board (Thonny, or
# mpremote cp blink.py :main.py), or paste it into the REPL. Change HALF_PERIOD_MS to prove a changed blink.
# Each line printed on USB serial carries the time, so the rate can be checked from the Mac.
import time
from machine import Pin

HALF_PERIOD_MS = 250
led = Pin("LED", Pin.OUT)   # GP25 on the Pico 2
n = 0
while True:
    led.toggle()
    print("blink", n, "half_period_ms", HALF_PERIOD_MS, "t_ms", time.ticks_ms())
    n += 1
    time.sleep_ms(HALF_PERIOD_MS)
