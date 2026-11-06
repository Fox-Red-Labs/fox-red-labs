// attachInterrupt on S1 (D2 = DIO10) with no hand on the button: the sketch flips the pad's own
// pull resistor (pull-down, then pull-up) so the input level changes, and counts CHANGE interrupts.
// S1 shorts DIO10 to ground only when pressed; released, the pull sets the level.
// Part A flips the pull with pinMode(); part B writes only the pull field of the pad's IOC register.
const pin_size_t interruptPin = 2;
const uint32_t IOC10 = 0x40003000u + 0x100u + 4u * 10u;   // IOC_BASE + IOC_O_IOC0 + 4 * DIO
const uint32_t PULL_M = 0x6000u, PULL_UP = 0x4000u, PULL_DOWN = 0x2000u, EDGE_M = 0x30000u;
volatile uint32_t count = 0;

void onChange() { count++; }

static void setPull(uint32_t pull) {
  volatile uint32_t *r = (volatile uint32_t *)IOC10;
  *r = (*r & ~PULL_M) | pull;
}

void setup() {
  Serial.begin(115200);
  delay(100);
  pinMode(interruptPin, INPUT_PULLUP);
  attachInterrupt(interruptPin, onChange, CHANGE);
  Serial.print("IRQ attached, IOC10=0x"); Serial.println(*(volatile uint32_t *)IOC10, HEX);

  // Part A: pinMode() between pull-down and pull-up, 5 cycles
  for (int i = 0; i < 5; i++) {
    pinMode(interruptPin, INPUT_PULLDOWN); delay(5);
    pinMode(interruptPin, INPUT_PULLUP); delay(5);
  }
  Serial.print("A pinMode cycles=5 count="); Serial.print(count);
  Serial.print(" IOC10=0x"); Serial.println(*(volatile uint32_t *)IOC10, HEX);

  // Part B: attach again, then flip only the pull bits, 5 cycles
  count = 0;
  attachInterrupt(interruptPin, onChange, CHANGE);
  for (int i = 0; i < 5; i++) {
    setPull(PULL_DOWN); delay(5);
    int lo = digitalRead(interruptPin);
    setPull(PULL_UP); delay(5);
    int hi = digitalRead(interruptPin);
    Serial.print("B cycle "); Serial.print(i); Serial.print(" low-read="); Serial.print(lo);
    Serial.print(" high-read="); Serial.print(hi); Serial.print(" count="); Serial.println(count);
  }
  Serial.print("B pull cycles=5 count="); Serial.print(count);
  Serial.print(" IOC10=0x"); Serial.println(*(volatile uint32_t *)IOC10, HEX);
}

void loop() {
  static uint32_t last = 0xffffffffu;
  if (count != last) { last = count; Serial.print("count="); Serial.println(count); }
  delay(500);
}
