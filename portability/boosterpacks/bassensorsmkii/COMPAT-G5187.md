# BP-BASSENSORSMKII on the LP-MSPM0G5187 with the BP-CC1407P-9-FE also stacked: which sensor works, by pin

Terms: [docs/vocab.md](../../../docs/vocab.md). Written 2026-10-20 before the first run of this code. Sources: SLAU829 (sensor pack user guide, Tables 1 to 6, Figure 9), SLAU967 Figure 2-3 (G5187 header), `data/boosterpacks.json` clash rows, SWRU633 Table 2-1 (radio pins).

## Which sensor works, by pin

The G5187 as shipped carries pins only on J1 (BoosterPack pins 1 to 10) and J3 (21 to 30). J2 and J4 (11 to 20, 31 to 40) are not fitted.

| Sensor | Pins it needs | Reachable on the G5187 | Clash with the CC1407P pack | Expected |
|---|---|---|---|---|
| HDC2080 (0x40) | 9, 10 I2C; 6 power (low = on) | Yes: PA9, PA8; PB22 | Pin 6 is the radio's optional DIO7 (output when the radio runs) | Works |
| OPT3001 (0x44) | 9, 10; 27 power (high = on) | Yes: PA15 | Pin 27 is the radio's DIO2, which drives the RF switch | Works while the radio is idle |
| TMP117 (0x48) | 9, 10; 28 power (high = on); ribbon cable J5 to J6 | Yes: PA12 | Pin 28 is the radio's DIO0 | Works only with the ribbon cable seated; not working on 2026-10-10 |
| DRV5055 | 2 output (PA25, ADC); 24 power (low = on, PB15) | Yes | Pin 2 is DIO4 and pin 24 is DIO3 (RF switch) | Works while the radio is idle |
| BMI160 (0x69) with BMM150 behind it | 9, 10 only; always powered | Yes | None | Did not answer on 2026-10-10; unknown |
| Interrupt and alert lines | 5 (BMI160 INT1), 8 (BMM150), 23 (HDC2080), 25 (OPT3001), 26 (TMP117) | Yes (J1, J3) | 23 is the radio's SPI_INT, 25 is DIO6 | Not used; this code polls |
| Motion INT2 (13), red LED (39), green LED (40) | J2 and J4 | No: no pins fitted | None | Unreachable without wires |

## Which pins collide

The radio pack needs SPI on pins 7, 14, 15, 18, interrupt on 23 and reset on 36. Pins 14, 15, 18 and 36 are on J2 and J4, so the radio cannot be driven from this LaunchPad at all with the shipped headers: it sits on the stack inert. Its optional outputs (pins 2, 6, 19, 24, 25, 27, 28) share six lines with the sensor pack. With the radio not started they are not driven by anything known (the CC1407P data sheet is advance information; the power-up state of its DIO lines is not confirmed here). The sensor pack's four power switches (6, 24, 27, 28) are the only place a clash can show: a radio output on one of them would switch a sensor on or off. If a sensor reads fine with both packs stacked, there is no clash in practice.

Pin 6 (PB22) is also the LaunchPad kit's microphone input (A0.24), and pin 5 (PB21) also reaches the USB VBUS divider through the J23 shunt. Neither matters for polling the sensors; the audio code and this code cannot run a microphone and the HDC2080 power switch together.

## Verdict, written before the run

Four of the six chips can be reached and powered with the radio pack stacked: HDC2080, OPT3001, DRV5055 and, with the ribbon cable, TMP117; the BMI160 needs only the two I2C pins. The two LEDs and the motion INT2 line cannot be reached. Prediction for the run: HDC2080, OPT3001 and DRV5055 read; TMP117 and BMI160 stay silent as on 2026-10-10 unless the stack or the code changed something.
