// Serial hello at 115200 on LP-MSPM0G3507 (UART0 to the XDS110 back-channel port).
void setup() {
  Serial.begin(115200);
  Serial.println("hello from LP-MSPM0G3507");
}

void loop() {
  static unsigned long n = 0;
  Serial.print("tick ");
  Serial.print(n++);
  Serial.print(" millis=");
  Serial.println(millis());
  delay(1000);
}
