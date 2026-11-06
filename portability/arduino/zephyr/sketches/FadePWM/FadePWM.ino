// analogWrite on D0 (PB27, green LED) through the Zephyr MSPM0 timer PWM driver (TIMA1 compare 1).
// The pin is checked from inside the chip: its input buffer is switched on and its level sampled
// 20,000 times per duty, so the fraction of high samples is the duty the pin really carries.
// Expected: about value/255 high (Zephyr PWM API, normal polarity: the pulse is the active-high time).
// Prints "PWM: PASS" when every measured duty is within 3 points of the set one and the pin is on
// the timer function. Then it fades up and down forever (Arduino's Fade).
#define IOMUX_PINCM58 (*(volatile uint32_t *)0x404280E8u)  /* PB27 */
#define GPIOB_DIN     (*(volatile uint32_t *)(0x400A2000u + 0x1380u))
#define TIMA1_LOAD    (*(volatile uint32_t *)(0x40862000u + 0x1808u))
#define TIMA1_CC1     (*(volatile uint32_t *)(0x40862000u + 0x1814u))
#define T1(off)       (*(volatile uint32_t *)(0x40862000u + (off)))
const pin_size_t led = 0;

static void dumpTimer() {
  uint32_t c1 = T1(0x1800), c2 = T1(0x1800);
  Serial.print("TIMA1 PWREN 0x"); Serial.print(T1(0x800), HEX);
  Serial.print(" CLKSEL 0x"); Serial.print(T1(0x1008), HEX);
  Serial.print(" CCLKCTL 0x"); Serial.print(T1(0x1108), HEX);
  Serial.print(" CCPD 0x"); Serial.print(T1(0x1100), HEX);
  Serial.print(" ODIS 0x"); Serial.print(T1(0x1104), HEX);
  Serial.print(" CTRCTL 0x"); Serial.print(T1(0x1804), HEX);
  Serial.print(" CCCTL1 0x"); Serial.print(T1(0x1834), HEX);
  Serial.print(" OCTL1 0x"); Serial.print(T1(0x1854), HEX);
  Serial.print(" CCACT0 0x"); Serial.print(T1(0x1870), HEX);
  Serial.print(" CCACT1 0x"); Serial.print(T1(0x1874), HEX);
  Serial.print(" CC0 "); Serial.print(T1(0x1810));
  Serial.print(" CTR "); Serial.print(c1); Serial.print(" then "); Serial.println(c2);
}

static int measure() {
  int high = 0;
  for (int i = 0; i < 20000; i++) {
    high += (GPIOB_DIN >> 27) & 1;
    for (volatile int d = 0; d < 7; d++) {}
  }
  return (high + 100) / 200;  // percent
}

void setup() {
  Serial.begin(115200);
  delay(300);
  Serial.println("FadePWM start");
  bool ok = true;
  int values[] = {0, 32, 64, 128, 192, 255};
  for (int v : values) {
    analogWrite(led, v);
    IOMUX_PINCM58 |= 0x00040000u;   // input enable, so the pin level can be read back
    delay(20);
    int pct = measure();
    int want = (v * 100 + 127) / 255;
    Serial.print("value "); Serial.print(v);
    Serial.print(" set "); Serial.print(want);
    Serial.print("% measured "); Serial.print(pct);
    Serial.print("% PINCM58 0x"); Serial.print(IOMUX_PINCM58, HEX);
    Serial.print(" LOAD "); Serial.print(TIMA1_LOAD);
    Serial.print(" CC1 "); Serial.println(TIMA1_CC1);
    int diff = pct - want;
    if (diff < -3 || diff > 3) ok = false;
    if ((IOMUX_PINCM58 & 0x3F) != 9) ok = false;   // TIMA1.CCP1 on the G351x
  }
  dumpTimer();
  // Check of the method: the same pin as a plain GPIO output, high then low, read the same way.
  pinMode(led, OUTPUT);
  digitalWrite(led, HIGH);
  IOMUX_PINCM58 |= 0x00040000u;
  int hi = measure();
  digitalWrite(led, LOW);
  int lo = measure();
  Serial.print("method check, GPIO high reads "); Serial.print(hi);
  Serial.print("%, low reads "); Serial.print(lo); Serial.println("%");
  Serial.println(ok ? "PWM: PASS" : "PWM: FAIL");
}

void loop() {
  for (int v = 0; v <= 255; v += 5) { analogWrite(led, v); delay(10); }
  for (int v = 255; v >= 0; v -= 5) { analogWrite(led, v); delay(10); }
}
