# Waveshare 3.5 inch panel (ST7796S screen over SPI, FT6336U touch over I2C) for the
# MicroPython port on the LP-MSPM0G3519. Written from the ST7796S and FT6336U data sheets.
# Wiring: SPI1 SCLK PB31, MOSI PB8, MISO PB7, CS PB12, DC PB9, BL PC4, RST PC5;
# touch SDA PC3, SCL PC2 (software I2C, internal pull-ups), INT PB14, RST PB11.
import time
from machine import Pin, SPI, SoftI2C

W = 320
H = 480

def rgb(r, g, b):
    return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3)

# 5x7 font, ASCII 32 to 95 (upper case, digits, punctuation), 5 column bytes per character, bit 0 is the top row
FONT = (b'\x00\x00\x00\x00\x00\x00\x00\x5f\x00\x00\x00\x07\x00\x07\x00\x14\x7f\x14\x7f\x14'
 b'\x24\x2a\x7f\x2a\x12\x23\x13\x08\x64\x62\x36\x49\x55\x22\x50\x00\x05\x03\x00\x00'
 b'\x00\x1c\x22\x41\x00\x00\x41\x22\x1c\x00\x14\x08\x3e\x08\x14\x08\x08\x3e\x08\x08'
 b'\x00\x50\x30\x00\x00\x08\x08\x08\x08\x08\x00\x60\x60\x00\x00\x20\x10\x08\x04\x02'
 b'\x3e\x51\x49\x45\x3e\x00\x42\x7f\x40\x00\x42\x61\x51\x49\x46\x21\x41\x45\x4b\x31'
 b'\x18\x14\x12\x7f\x10\x27\x45\x45\x45\x39\x3c\x4a\x49\x49\x30\x01\x71\x09\x05\x03'
 b'\x36\x49\x49\x49\x36\x06\x49\x49\x29\x1e\x00\x36\x36\x00\x00\x00\x56\x36\x00\x00'
 b'\x08\x14\x22\x41\x00\x14\x14\x14\x14\x14\x00\x41\x22\x14\x08\x02\x01\x51\x09\x06'
 b'\x32\x49\x79\x41\x3e\x7e\x11\x11\x11\x7e\x7f\x49\x49\x49\x36\x3e\x41\x41\x41\x22'
 b'\x7f\x41\x41\x22\x1c\x7f\x49\x49\x49\x41\x7f\x09\x09\x09\x01\x3e\x41\x49\x49\x7a'
 b'\x7f\x08\x08\x08\x7f\x00\x41\x7f\x41\x00\x20\x40\x41\x3f\x01\x7f\x08\x14\x22\x41'
 b'\x7f\x40\x40\x40\x40\x7f\x02\x0c\x02\x7f\x7f\x04\x08\x10\x7f\x3e\x41\x41\x41\x3e'
 b'\x7f\x09\x09\x09\x06\x3e\x41\x51\x21\x5e\x7f\x09\x19\x29\x46\x46\x49\x49\x49\x31'
 b'\x01\x01\x7f\x01\x01\x3f\x40\x40\x40\x3f\x1f\x20\x40\x20\x1f\x3f\x40\x38\x40\x3f'
 b'\x63\x14\x08\x14\x63\x07\x08\x70\x08\x07\x61\x51\x49\x45\x43\x00\x7f\x41\x41\x00'
 b'\x02\x04\x08\x10\x20\x00\x41\x41\x7f\x00\x04\x02\x01\x02\x04\x40\x40\x40\x40\x40')

class Screen:
    def __init__(self, baud=20000000):
        self.spi = SPI(1, baudrate=baud)
        self.cs = Pin("B12", Pin.OUT, value=1)
        self.dc = Pin("B9", Pin.OUT, value=1)
        self.rst = Pin("C5", Pin.OUT, value=1)
        self.bl = Pin("C4", Pin.OUT, value=0)
        self.init()

    def cmd(self, c, data=None):
        self.cs(0)
        self.dc(0)
        self.spi.write(bytes((c,)))
        self.dc(1)
        if data:
            self.spi.write(data)
        self.cs(1)
        time.sleep_us(20)   # the level shifter on the module drops a chip-select pulse shorter than this

    def init(self):
        self.rst(0)
        time.sleep_ms(10)
        self.rst(1)
        time.sleep_ms(120)
        self.cmd(0x01)
        time.sleep_ms(120)
        self.cmd(0x11)
        time.sleep_ms(120)
        self.cmd(0x36, b'\x48')     # portrait, BGR
        self.cmd(0x3A, b'\x55')     # 16 bits per pixel
        self.cmd(0x21)              # inversion on (IPS panel)
        self.cmd(0x29)
        time.sleep_ms(20)
        self.bl(1)

    def window(self, x0, y0, x1, y1):
        self.cmd(0x2A, bytes((x0 >> 8, x0 & 255, x1 >> 8, x1 & 255)))
        self.cmd(0x2B, bytes((y0 >> 8, y0 & 255, y1 >> 8, y1 & 255)))

    def _begin(self):
        self.cs(0)
        self.dc(0)
        self.spi.write(b'\x2c')
        self.dc(1)

    def rect(self, x, y, w, h, color):
        if w <= 0 or h <= 0:
            return
        self.window(x, y, x + w - 1, y + h - 1)
        row = bytes((color >> 8, color & 255)) * w
        self._begin()
        self.spi.write_repeat(row, h)
        self.cs(1)
        time.sleep_us(20)

    def fill(self, color):
        self.rect(0, 0, W, H, color)

    def fill_loop(self, color):
        # the same fill with one plain spi.write per row, to time the cost of calling from Python
        self.window(0, 0, W - 1, H - 1)
        row = bytes((color >> 8, color & 255)) * W
        self._begin()
        w = self.spi.write
        for _ in range(H):
            w(row)
        self.cs(1)
        time.sleep_us(20)

    def text(self, s, x, y, fg, bg=0, scale=1):
        cw = 6 * scale
        n = len(s)
        w = cw * n
        h = 8 * scale
        self.window(x, y, x + w - 1, y + h - 1)
        fgb = bytes((fg >> 8, fg & 255))
        bgb = bytes((bg >> 8, bg & 255))
        # one scan line at a time: row r of every character
        self._begin()
        for r in range(8):
            line = bytearray()
            for ch in s:
                i = ord(ch)
                if 96 <= i <= 122:
                    i -= 32
                i -= 32
                if i < 0 or i > 63:
                    i = 0
                cols = FONT[i * 5:i * 5 + 5]
                for c in cols:
                    line += (fgb if (c >> r) & 1 else bgb) * scale
                line += bgb * scale
            for _ in range(scale):
                self.spi.write(line)
        self.cs(1)
        time.sleep_us(20)

class Touch:
    def __init__(self):
        self.rst = Pin("B11", Pin.OUT, value=1)
        self.irq = Pin("B14", Pin.IN, Pin.PULL_UP)
        self.i2c = SoftI2C(Pin("C2"), Pin("C3"), freq=400000)
        self.rst(0)
        time.sleep_ms(10)
        self.rst(1)
        time.sleep_ms(300)
        self.buf = bytearray(5)

    def ids(self):
        # vendor 0xA8 (0x11), chip 0xA3 (0x64 for the FT6336U), firmware 0xA6
        return tuple(self.i2c.readfrom_mem(0x38, r, 1)[0] for r in (0xA8, 0xA3, 0xA6))

    def read(self):
        # returns (x, y) in panel pixels, or None when nothing touches
        self.i2c.readfrom_mem_into(0x38, 0x02, self.buf)
        b = self.buf
        if (b[0] & 15) == 0:
            return None
        return (((b[1] & 15) << 8) | b[2], ((b[3] & 15) << 8) | b[4])
