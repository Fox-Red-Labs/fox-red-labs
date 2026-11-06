"""Physical reference/threshold contracts for the unflashed DRV8376 port."""
import ast
from pathlib import Path
import re
import unittest

ROOT = Path(__file__).resolve().parents[1]
HEADERS = [ROOT / "common_files/hal/gate_driver_interface/drv8376/drv8376_interface.h",
           ROOT / "common_files/app_lib/include/motor_params.h"]
DEFS = {}
for path in HEADERS:
    source = re.sub(r"//[^\n]*", "", path.read_text())
    DEFS.update(re.findall(r"^#define\s+(\w+)\s+(\([^\n]+\))\s*$", source, re.M))


def value(name):
    expression = re.sub(r"(?<=\d)f\b", "", DEFS[name])
    expression = re.sub(r"\b[A-Z][A-Z0-9_]+\b", lambda m: str(value(m[0])), expression)
    tree = ast.parse(expression, mode="eval")
    allowed = (ast.Expression, ast.BinOp, ast.UnaryOp, ast.Constant, ast.Add,
               ast.Sub, ast.Mult, ast.Div, ast.USub, ast.UAdd)
    assert all(isinstance(node, allowed) for node in ast.walk(tree))
    return eval(compile(tree, "<header constants>", "eval"), {"__builtins__": {}})


class ReferenceLimits(unittest.TestCase):
    def test_known_sink_current_is_negative_in_foc(self):
        signed = 100 * value("DRV8376_CURRENT_SF") * value("DRV8376_SIGN_CURRENT_SF")
        self.assertLess(signed, 0)  # Measured OUT-to-GND current raises CSA output.
        threshold = 1900 - 1.0 / (value("DRV8376_CURRENT_SF") * value("DRV8376_SIGN_CURRENT_SF"))
        self.assertGreater(threshold, 1900)
        self.assertLess(threshold, 4095)

    def test_external_three_volt_zero_current(self):
        # REF3130 produces 1.5 V at zero current, not the ADC midpoint of 1.65 V.
        self.assertAlmostEqual(value("DRV8376_CSA_VREF_V"), 3.0)
        counts = value("DRV8376_IDC_OFFSET_AD")
        self.assertGreater(counts, 1861)
        self.assertLess(counts, 1863)
        self.assertAlmostEqual(counts * value("DRV8376_CURRENT_SF"), 1.5)

    def test_trip_remains_inside_amplifier_linear_output(self):
        zero = value("DRV8376_CSA_VREF_V") / 2
        swing = value("MOTOR_BLY171S_OVER_CURRENT_LIM_A") * value("DRV8376_CSA_GAIN_V_PER_A")
        self.assertGreater(zero - swing, 0.25)
        self.assertLess(zero + swing, value("DRV8376_CSA_VREF_V") - 0.25)

    def test_initial_alignment_below_hardware_trip(self):
        self.assertLess(value("MOTOR_BLY171S_ALIGN_CURRENT_A"),
                        value("MOTOR_BLY171S_OVER_CURRENT_LIM_A"))


if __name__ == "__main__":
    unittest.main()
