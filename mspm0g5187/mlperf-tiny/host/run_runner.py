#!/usr/bin/env python3
"""Run the unmodified MLPerf Tiny host runner (third-party/mlperf-tiny/runner) against ONE named serial port.
The runner's own device scan opens every USB serial port it finds and sends `name%` to it; on a bench with many
boards that would talk to the wrong ones, so this wrapper replaces only that scan with the port given here.
Everything else (scripts, dataset reader, timing, accuracy and throughput summaries) is the runner's own code.
  python3 run_runner.py <port> <baud> <p|a> <dataset dir holding kws01/> <test yaml>
Run from a scratch folder: the runner writes sessions/<time>/log.txt, results.txt and results.json there."""
import sys, os, yaml, json
R = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..", "..", "third-party", "mlperf-tiny", "runner")
sys.path.insert(0, R)
port, baud, mode, data, ty = sys.argv[1], int(sys.argv[2]), sys.argv[3], sys.argv[4], sys.argv[5]
import main, script
from device_under_test import DUT
class OnePort(dict):
    def __init__(self, cfg, desired_baud, mode):
        super().__init__(); self.b = desired_baud
    def scan(self):
        self["dut"] = {"type": "dut", "instance": DUT(port, self.b)}
main.DeviceManager = OnePort
dut_cfg = {"name": "lp-mspm0g5187", "type": "dut", "baud": baud}
res, power = main.run_test([dut_cfg], dut_cfg, yaml.safe_load(open(ty)), data, mode)
for r in res: r["mode"] = mode
rf = os.path.join(os.path.dirname(script.log_filename), "results.txt")
main.summarize_result(res, power, mode=mode, results_file=rf)
json.dump(res, open(os.path.join(os.path.dirname(script.log_filename), "results.json"), "w"), default=str)
print("session", os.path.dirname(script.log_filename))
