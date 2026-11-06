#!/usr/bin/env python3
"""Adafruit Blinka on the AM62L EVM: blink LD8 and read SW5. Run gpio_claim.sh free first.
  blinka_demo.py SECONDS"""
import sys, time
import board, digitalio

print('board id:', board.board_id if hasattr(board, 'board_id') else '?')
led = digitalio.DigitalInOut(board.LED)
led.direction = digitalio.Direction.OUTPUT
btn = digitalio.DigitalInOut(board.BUTTON)
btn.direction = digitalio.Direction.INPUT
t0 = time.time(); last = btn.value; on = False
print('button value at start:', btn.value, flush=True)
while time.time() - t0 < float(sys.argv[1]):
    on = not on          # LD8's pad has its input buffer off, so led.value reads back 0: keep the state here
    led.value = on
    v = btn.value
    if v != last:
        print(f'{time.time()-t0:6.2f} button {"released" if v else "pressed"}', flush=True)
        last = v
    print(f'{time.time()-t0:6.2f} set LD8 {"on" if on else "off"}', flush=True)
    time.sleep(0.25)
