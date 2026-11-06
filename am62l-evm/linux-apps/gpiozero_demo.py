#!/usr/bin/env python3
"""gpiozero on the AM62L EVM through the lgpio pin factory.
LD8 = GPIO0_123 and SW5 = GPIO0_90, both on gpiochip0 (600000.gpio). Run gpio_claim.sh free first.
  gpiozero_demo.py blink SECONDS        LD8 on/off, 2 Hz (the pad's input buffer is off, so reading the pin back gives 0: use gpio_regs.py)
  gpiozero_demo.py button SECONDS       prints every press and release of SW5
gpiozero numbers pins as BCM GPIO numbers; here that number is the line offset on chip 0."""
import sys, time
from gpiozero import Device, LED, Button
from gpiozero.pins import BoardInfo, HeaderInfo, PinInfo
from gpiozero.pins.lgpio import LGPIOFactory


class AM62LFactory(LGPIOFactory):
    """gpiozero 2.0 wants a board description to resolve pin numbers and only knows Raspberry Pi
    revisions. This one describes the two GPIO0 lines used here (LD8 and SW5); other lines are added
    by listing their offsets in LINES."""
    LINES = (90, 123)

    def _get_board_info(self):
        pins = {}
        for i, n in enumerate(self.LINES, 1):
            names = frozenset({f'GPIO{n}', n, str(n), f'BCM{n}'})
            pins[i] = PinInfo(number=i, name=f'GPIO{n}', names=names, pull='', row=i, col=1, interfaces=frozenset({'gpio'}))
        hdr = HeaderInfo(name='GPIO0', rows=len(pins), columns=1, pins=pins)
        return BoardInfo(revision='am62l', model='TMDS62LEVM', pcb_revision='', released='', soc='AM62L32',
                         manufacturer='TI', memory=2048, storage='SD', usb=2, usb3=0, ethernet=2, eth_speed=1000,
                         wifi=False, bluetooth=False, csi=0, dsi=1, headers={'GPIO0': hdr}, board={})


Device.pin_factory = AM62LFactory(chip=0)
mode, secs = sys.argv[1], float(sys.argv[2])
if mode == 'blink':
    led = LED(123)
    t0 = time.time(); on = False
    while time.time() - t0 < secs:
        on = not on
        led.on() if on else led.off()
        print(f'{time.time()-t0:6.2f} set LD8 {"on" if on else "off"}', flush=True)
        time.sleep(0.25)
elif mode == 'button':
    b = Button(90, pull_up=None, active_state=False)   # SW5 pulls the line low when pressed (device tree: active low)
    b.when_pressed = lambda: print(f'{time.time():.3f} pressed', flush=True)
    b.when_released = lambda: print(f'{time.time():.3f} released', flush=True)
    print('is_pressed at start:', b.is_pressed, flush=True)
    time.sleep(secs)
