<!-- experiment
id: E177
board: LP-MSPM0G5187
date: 2026-10-10
status: parked
reached: run
line: The room-occupancy model runs on the NPU once a second. Its input is a live light reading from the BP-BASSENSORSMKII's OPT3001, taken over I2C. Each call takes 1.30 to 1.32 ms, within 1 percent of the time on stored test vectors. The bench was lit at 316 to 319 lux, source not recorded. The model reads 0 people for the first 15 samples while the window fills, then 1 person from sample 18 on. Not done: the covered-sensor check (expect 0). Three of the model's four inputs are fixed at empty-room values because this BoosterPack has no sound, CO2 or motion sensor.
stop: none
-->
# Can the LP-MSPM0G5187's NPU count people from a live light reading on the BP-BASSENSORSMKII?

**Short answer.** It runs. The 636-parameter room-occupancy model reads the Sensors BoosterPack's OPT3001 light sensor over I2C once a second, runs on the TinyEngine NPU in 1.30 to 1.32 ms (within 1 percent of its time on stored test vectors), and prints the count on the serial port. With the bench lit as it was (316 to 319 lux, the light source not recorded) it reads 0 people for 15 samples while the window fills, then 1 person from sample 18 on. Whether the count follows the light is untested: no run varied it. **Unknown (suspected: setup or handling, or the model's training range).** Two things could be behind the "1 person", and neither is tested: this repo's program holds the three inputs this BoosterPack lacks (sound, CO2, motion) at empty-room values, and 317 lux is twice the brightest window the model was trained on. This shows a real sensor driving the NPU. It does not measure occupancy.

Terms: [vocab](../vocab.md) (NPU, TinyEngine, OPT3001, lux, logits, SysTick, soft float, the BoosterPack's sensors). Board page: [LP-MSPM0G5187 README](../../mspm0g5187/README.md), [FAQ](../../mspm0g5187/FAQ.md). Its time on the NPU with stored data: [NPU against CPU on the LP-MSPM0G5187](2026-10-09-mspm0g5187-npu-vs-cpu.md). Run 2026-10-10 on an Intel Mac, macOS 15.7, MSPM0 SDK 2.11.00.07, TI Arm Clang 5.1.1, SysConfig 1.26.3, pyserial 3.5.

**Question.** Every NPU number came from test vectors baked into the image. The Sensors BoosterPack (BP-BASSENSORSMKII, six sensors) arrived, and the question was whether one of its readings could feed the model on the chip, in the window layout the model was trained on, without changing the model.

**Vendor position.** TI's NPU examples for this board take sensor data from a different BoosterPack, the TIDA-010997 with a BMI270 motion sensor over SPI. That example is `fall_detection_ai_npu/README.md` in MSPM0 SDK 2.11.00.07, under `examples/nortos/LP_MSPM0G5187/edgeAI/`. The other source is TI's Edge AI Studio tool over serial at 115200, described in `timeseries_data_capture/README.md` in the same folder. None uses the BP-BASSENSORSMKII. TI's user guide for the BoosterPack (SLAU829, May 2020) lists the OPT3001 at I2C address 0x44, powered when header pin 27 is high.

**Setup.** The BoosterPack sits on the LaunchPad with its pin 1 (marked 3V3) in pin 1 of J1 on the LaunchPad development kit. No wires, no jumpers. The program drives pin 27 (PA15) high to power the light sensor; the I2C bus is on header pins 9 (PA9, clock) and 10 (PA8, data). Flash the program with UniFlash's DSLite and read the LaunchPad kit's backchannel serial port at 115200 baud, 8N1; 48 seconds of output cover the window fill and the steady state. The model has 636 parameters.

**What was tried.** The model takes 4 channels of 32 samples: light in lux, sound in volts, CO2 in ppm, and a motion flag. The program keeps a 128-value window. Every second it reads the OPT3001's result register, converts it to lux (exponent and mantissa, as the OPT3001 data sheet gives it), shifts the light channel by one and puts the new reading last. Sound, CO2 and motion stay at 0.08 V, 355 ppm and 0, the medians of the empty-room windows in the model's 20 test vectors. The light channel starts at 0, so the first 31 calls see a window that is part dark, part lit. Then it calls the NPU, waits for the NPU's done interrupt, counts the cycles with SysTick, picks the largest of the four logits, lights LED1 when the count is not 0, and prints one line: sample number, lux, the four logits, the count, the cycles. The window moves one sample a second instead of one every 30 s as in the dataset.

**How far it got.** Run, on the bench in the room light of the afternoon (316 to 319 lux, the source not recorded), no one touching the board. 48 lines were saved from the first sample; an earlier 45-line run from sample 31 on agrees.

| What | Reading |
|---|---|
| Light at the sensor | 316 to 319 lux, steady |
| NPU time per call | 1.30 to 1.32 ms at 80 MHz (104k to 106k cycles); the stored-vector run measured 1.292 ms on its fastest vector and 1.323 ms on its slowest |
| Count, samples 1 to 15 | 0 |
| Count, sample 16 | 1, then 0 again at 17 |
| Count, samples 18 to 48 | 1 |
| Logits from sample 32 on (window full) | 21, 25, -45, -128 for 0, 1, 2 and 3 people |

The logits are the model's int8 outputs, so -128 is the floor. The win for "1 person" over "0" is 3 to 4 units (24 or 25 against 21) on a scale where the top logit sits 152 to 153 above the floor: a narrow call that small input changes could flip. Program size from the linker map: 11.7 kB of flash; in RAM 1.1 kB of data plus a 2 kB stack.

The dataset's lit, occupied windows sit at 115 to 157 lux and its empty ones at 0 to 10 (the same 20 test vectors), so 317 lux is above anything the model saw in training. It still lands on the lit side.

The cycle count climbs by about 1.6 percent from the first call to the 32nd, then stays at exactly the same count from sample 32 on, although the lux still wanders by 1 to 2. **Unknown.** Sample 32 is when the last zero leaves the window, so the time depends on the values in the window somehow; where the NNC build converts the float inputs was not looked at.

**The line.** None reached. The loop is a one-second busy wait plus the read, the NPU call and the print; the chip never sleeps, and the loop's own time was not measured.

**What limits the answer.** **Unknown (suspected: setup or handling, or the model's training range).** Only one of the four inputs is a real sensor. In the model's 20 test windows CO2 and light are both high when people are in, so a lit room with CO2 pinned at the empty-room value (the program's choice) is a mix the training data never had; and 317 lux is twice the brightest window it saw (the model's range). Expect it to say occupied whenever the light is on. Not tested. A covered sensor (0 lux, inside the training range) checks the first cause only; a dimmed or shaded light at 115 to 157 lux is the check that separates the two.

**Ask.** Anyone at the bench: cover the light sensor for a minute and read the count on the serial line (expected: 0 within 32 samples). Then dim or shade the light until the sensor reads 115 to 157 lux and read it again: a 1 there means the fixed channels alone give the answer, a 0 means the out-of-range light did.

**What a reader should take.** Feeding the NPU from a live sensor is a 100-line program: read, shift, call, print. The model's time does not change with real data. What changes is what the number means, and that is set by which sensors the training data had, not by the chip.
