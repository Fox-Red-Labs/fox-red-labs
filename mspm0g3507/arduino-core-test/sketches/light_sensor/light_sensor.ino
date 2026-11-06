// Reads the LaunchPad light sensor with analogRead.
// The sensor is on PA22 (jumper J16). The core calls PA22 "A7" (Arduino pin 32, ADC0 channel 7).
// The core's analogRead returns 12-bit counts (0 to 4095) with its default settings: not checked on a board.
void setup() {
  Serial.begin(115200);
}

void loop() {
  int raw = analogRead(A7);
  Serial.print("light A7 raw=");
  Serial.println(raw);
  delay(500);
}
