// attachInterrupt on button S2 (D4 = PB3) of the LP-MSPM0G3519 with no hand on the button:
// the sketch moves the pin's internal pull between down and up, which makes an edge on an open button.
// 10 pull cycles: RISING expects 10 calls, FALLING 10, CHANGE 20 (Arduino attachInterrupt semantics).
// Prints "INTERRUPT: PASS" when all three counts match. The GPIOB edge-select register for PB3
// (POLARITY15_0 bits 7:6, 01 rising, 10 falling, 11 both) is printed after each attach.
// Last, FALLING again after clearing those two bits by hand ("WITH-CLEAR").
#define GPIOB_POLARITY15_0 (*(volatile uint32_t *)(0x400A2000u + 0x1390u))
const pin_size_t btn = 4;
volatile int calls = 0;
void onEdge() { calls++; }

static int cycle(PinMode up, PinMode down, int n) {
  int highs = 0;
  for (int i = 0; i < n; i++) {
    pinMode(btn, up);   delay(5); highs += digitalRead(btn) == HIGH;
    pinMode(btn, down); delay(5);
  }
  return highs;
}

static bool run(const char *name, PinStatus mode, int expect) {
  pinMode(btn, INPUT_PULLDOWN);
  delay(10);
  int start = digitalRead(btn);
  calls = 0;
  attachInterrupt(btn, onEdge, mode);
  uint32_t pol = (GPIOB_POLARITY15_0 >> 6) & 3;
  int highs = cycle(INPUT_PULLUP, INPUT_PULLDOWN, 10);
  delay(10);
  detachInterrupt(btn);
  int got = calls;
  Serial.print(name); Serial.print(": level at start "); Serial.print(start);
  Serial.print(", highs read "); Serial.print(highs);
  Serial.print(", edge bits "); Serial.print(pol);
  Serial.print(", interrupts "); Serial.print(got);
  Serial.print(" (expect "); Serial.print(expect); Serial.println(")");
  return got == expect && start == 0 && highs == 10;
}

void setup() {
  Serial.begin(115200);
  delay(300);
  Serial.println("ButtonInterrupt start");
  bool ok = run("RISING", RISING, 10);
  ok &= run("FALLING", FALLING, 10);
  ok &= run("CHANGE", CHANGE, 20);
  Serial.println(ok ? "INTERRUPT: PASS" : "INTERRUPT: FAIL");
  GPIOB_POLARITY15_0 &= ~(3u << 6);
  bool ok2 = run("FALLING after clear", FALLING, 10);
  pinMode(btn, INPUT_PULLUP);
  Serial.println(ok2 ? "WITH-CLEAR: PASS" : "WITH-CLEAR: FAIL");
}

void loop() {
  delay(1000);
}
