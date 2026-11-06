// analogWrite() on the red LED (D1 = DIO14, LGPT1 channel 2) and the green LED (D0 = DIO15, LGPT2 channel 0),
// then the timer registers read back: TGT (period), CnCC (compare), CNTR (running counter).
// The Fade loop of Arduino's 01.Basics/Fade follows, on the red LED.
const uint32_t LGPT1 = 0x40061000u, LGPT2 = 0x40062000u;
const uint32_t O_CTL = 0x0C, O_CNTR = 0x14, O_C0CFG = 0xC0, O_C2CFG = 0xC8, O_TGT = 0x13C, O_C0CC = 0x140, O_C2CC = 0x148;
static uint32_t rd(uint32_t a) { return *(volatile uint32_t *)a; }

static void dump(const char *tag, uint32_t base, uint32_t cc, uint32_t cfg) {
  uint32_t c1 = rd(base + O_CNTR), c2 = rd(base + O_CNTR);
  Serial.print(tag); Serial.print(" CTL=0x"); Serial.print(rd(base + O_CTL), HEX);
  Serial.print(" TGT="); Serial.print(rd(base + O_TGT));
  Serial.print(" CC="); Serial.print(rd(base + cc));
  Serial.print(" CFG=0x"); Serial.print(rd(base + cfg), HEX);
  Serial.print(" CNTR="); Serial.print(c1); Serial.print(","); Serial.println(c2);
}

int brightness = 0, fadeAmount = 5;

void setup() {
  Serial.begin(115200);
  delay(100);
  const int vals[] = {0, 64, 128, 255};
  for (int v : vals) {
    analogWrite(1, v); analogWrite(0, 255 - v); delay(20);
    Serial.print("value red="); Serial.print(v); Serial.print(" green="); Serial.println(255 - v);
    dump(" LGPT1 red C2", LGPT1, O_C2CC, O_C2CFG);
    dump(" LGPT1 red C0", LGPT1, O_C0CC, O_C0CFG);
    dump(" LGPT2 green C0", LGPT2, O_C0CC, O_C0CFG);
  }
  Serial.println("fade loop");
}

void loop() {
  analogWrite(1, brightness);
  brightness = brightness + fadeAmount;
  if (brightness <= 0 || brightness >= 255) { fadeAmount = -fadeAmount; }
  delay(30);
}
