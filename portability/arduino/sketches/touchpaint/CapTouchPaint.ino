/***************************************************
  This is our touchscreen painting example for the Adafruit ILI9341
  captouch shield
  ----> http://www.adafruit.com/products/1947

  Check out the links above for our tutorials and wiring diagrams

  Adafruit invests time and resources providing this open source code,
  please support Adafruit and open-source hardware by purchasing
  products from Adafruit!

  Written by Limor Fried/Ladyada for Adafruit Industries.
  MIT license, all text above must be included in any redistribution
 ****************************************************/


#include <Arduino_GFX_Library.h>    // MSPM0 port: Arduino_GFX instead of Adafruit_GFX + Adafruit_ILI9341
#include <SPI.h>       // this is needed for display
#include <Wire.h>      // this is needed for FT6206
#include <Adafruit_FT6206.h>

// The FT6206 uses hardware I2C (SCL/SDA)
Adafruit_FT6206 ctp = Adafruit_FT6206();

// MSPM0 port: the display (ST7796S 320x480 on the Waveshare module) through Arduino_GFX.
// Rotation 4 makes the pixel and touch coordinates agree. SPI mode 0 as in display-test.
class Arduino_ST7796_Mode0 : public Arduino_ST7796
{
public:
  using Arduino_ST7796::Arduino_ST7796;
  bool begin(int32_t speed = GFX_NOT_DEFINED) override { return Arduino_ST7796::begin(speed, SPI_MODE0); }
};
Arduino_DataBus *bus = new Arduino_HWSPI(PIN_LCD_DC, PIN_LCD_CS);
Arduino_GFX *gfx = new Arduino_ST7796_Mode0(bus, PIN_LCD_RST, 4 /* rotation */, true /* IPS */);
Arduino_GFX &tft = *gfx;   // the rest of the sketch keeps its tft.xxx() calls
#define ILI9341_BLACK RGB565_BLACK
#define ILI9341_WHITE RGB565_WHITE
#define ILI9341_RED RGB565_RED
#define ILI9341_YELLOW RGB565_YELLOW
#define ILI9341_GREEN RGB565_GREEN
#define ILI9341_CYAN RGB565_CYAN
#define ILI9341_BLUE RGB565_BLUE
#define ILI9341_MAGENTA RGB565_MAGENTA

// Size of the color selection boxes and the paintbrush size
#define BOXSIZE 40
#define PENRADIUS 3
int oldcolor, currentcolor;

void setup(void) {
  Serial.begin(115200);
  while (!Serial) delay(10);     // pause the serial port
 
  Serial.println(F("Cap Touch Paint!"));
  
  tft.begin();
  pinMode(PIN_LCD_BL, OUTPUT);          // MSPM0 port: backlight on
  digitalWrite(PIN_LCD_BL, HIGH);
  pinMode(PIN_TP_RST, OUTPUT);          // MSPM0 port: reset the touch chip first
  digitalWrite(PIN_TP_RST, LOW); delay(10);
  digitalWrite(PIN_TP_RST, HIGH); delay(300);

  if (! ctp.begin(40, &Wire)) {  // pass in 'sensitivity' coefficient and I2C bus
    Serial.println("Couldn't start FT6206 touchscreen controller");
    while (1) delay(10);
  }

  Serial.println("Capacitive touchscreen started");
  
  tft.fillScreen(ILI9341_BLACK);
  
  // make the color selection boxes
  tft.fillRect(0, 0, BOXSIZE, BOXSIZE, ILI9341_RED);
  tft.fillRect(BOXSIZE, 0, BOXSIZE, BOXSIZE, ILI9341_YELLOW);
  tft.fillRect(BOXSIZE*2, 0, BOXSIZE, BOXSIZE, ILI9341_GREEN);
  tft.fillRect(BOXSIZE*3, 0, BOXSIZE, BOXSIZE, ILI9341_CYAN);
  tft.fillRect(BOXSIZE*4, 0, BOXSIZE, BOXSIZE, ILI9341_BLUE);
  tft.fillRect(BOXSIZE*5, 0, BOXSIZE, BOXSIZE, ILI9341_MAGENTA);
 
  // select the current color 'red'
  tft.drawRect(0, 0, BOXSIZE, BOXSIZE, ILI9341_WHITE);
  currentcolor = ILI9341_RED;
}

void loop() {
  // Wait for a touch
  if (! ctp.touched()) {
    return;
  }

  // Retrieve a point  
  TS_Point p = ctp.getPoint();
  
 /*
  // Print out raw data from screen touch controller
  Serial.print("X = "); Serial.print(p.x);
  Serial.print("\tY = "); Serial.print(p.y);
  Serial.print(" -> ");
 */

  // MSPM0 port: no flip, with rotation 4 the touch chip reports screen coordinates directly.

  // Print out the remapped (rotated) coordinates
  Serial.print("("); Serial.print(p.x);
  Serial.print(", "); Serial.print(p.y);
  Serial.println(")");
  

  if (p.y < BOXSIZE) {
     oldcolor = currentcolor;

     if (p.x < BOXSIZE) { 
       currentcolor = ILI9341_RED; 
       tft.drawRect(0, 0, BOXSIZE, BOXSIZE, ILI9341_WHITE);
     } else if (p.x < BOXSIZE*2) {
       currentcolor = ILI9341_YELLOW;
       tft.drawRect(BOXSIZE, 0, BOXSIZE, BOXSIZE, ILI9341_WHITE);
     } else if (p.x < BOXSIZE*3) {
       currentcolor = ILI9341_GREEN;
       tft.drawRect(BOXSIZE*2, 0, BOXSIZE, BOXSIZE, ILI9341_WHITE);
     } else if (p.x < BOXSIZE*4) {
       currentcolor = ILI9341_CYAN;
       tft.drawRect(BOXSIZE*3, 0, BOXSIZE, BOXSIZE, ILI9341_WHITE);
     } else if (p.x < BOXSIZE*5) {
       currentcolor = ILI9341_BLUE;
       tft.drawRect(BOXSIZE*4, 0, BOXSIZE, BOXSIZE, ILI9341_WHITE);
     } else if (p.x <= BOXSIZE*6) {
       currentcolor = ILI9341_MAGENTA;
       tft.drawRect(BOXSIZE*5, 0, BOXSIZE, BOXSIZE, ILI9341_WHITE);
     }

     if (oldcolor != currentcolor) {
        if (oldcolor == ILI9341_RED) 
          tft.fillRect(0, 0, BOXSIZE, BOXSIZE, ILI9341_RED);
        if (oldcolor == ILI9341_YELLOW) 
          tft.fillRect(BOXSIZE, 0, BOXSIZE, BOXSIZE, ILI9341_YELLOW);
        if (oldcolor == ILI9341_GREEN) 
          tft.fillRect(BOXSIZE*2, 0, BOXSIZE, BOXSIZE, ILI9341_GREEN);
        if (oldcolor == ILI9341_CYAN) 
          tft.fillRect(BOXSIZE*3, 0, BOXSIZE, BOXSIZE, ILI9341_CYAN);
        if (oldcolor == ILI9341_BLUE) 
          tft.fillRect(BOXSIZE*4, 0, BOXSIZE, BOXSIZE, ILI9341_BLUE);
        if (oldcolor == ILI9341_MAGENTA) 
          tft.fillRect(BOXSIZE*5, 0, BOXSIZE, BOXSIZE, ILI9341_MAGENTA);
     }
  }
  if (((p.y-PENRADIUS) > BOXSIZE) && ((p.y+PENRADIUS) < tft.height())) {
    tft.fillCircle(p.x, p.y, PENRADIUS, currentcolor);
  }
}