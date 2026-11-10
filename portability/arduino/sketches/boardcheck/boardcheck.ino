/* A check of the Arduino layer that needs nothing wired: Serial, millis, micros, delay and GPIO.
   Serial (115200 8N1, XDS110 backchannel): the board name once, then once a second the uptime from
   millis() and micros() and the LED state. The first LED (LED_BUILTIN) toggles every 500 ms. */
void setup()
{
  Serial.begin(115200);
  pinMode(LED_BUILTIN, OUTPUT);
  Serial.print("boardcheck: Arduino layer on ");
  Serial.println(BOARD_NAME);
}

void loop()
{
  static uint32_t last = 0, blink = 0;
  static int led = 0;
  uint32_t now = millis();
  if (now - blink >= 500) {
    blink = now;
    led = !led;
    digitalWrite(LED_BUILTIN, led);
  }
  if (now - last >= 1000) {
    last = now;
    Serial.print("millis=");  Serial.print(now);
    Serial.print(" micros="); Serial.print(micros());
    Serial.print(" led=");    Serial.println(led);
  }
}
