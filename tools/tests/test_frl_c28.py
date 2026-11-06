"""The C2000 route's own pieces (tools/frl/build_c28.py, the rate-aware pattern check), with no board."""
import sys
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools" / "frl"))
import build_c28  # noqa: E402
import verify  # noqa: E402
from core import Stop  # noqa: E402

# The loop of TI's led_ex1_blinky.c for the F280013x (C2000Ware 26.02.00.00), as shipped.
TI_MAIN = """void main(void)
{
    Device_init();
    for(;;)
    {
        //
        // Turn on LED
        //
        GPIO_writePin(myBoardLED0_GPIO, 0);
        //
        // Delay for a bit.
        //
        DEVICE_DELAY_US(500000);
        //
        // Turn off LED
        //
        GPIO_writePin(myBoardLED0_GPIO, 1);
        //
        // Delay for a bit.
        //
        DEVICE_DELAY_US(500000);
    }
}
"""


def sample(levels, rate):
    """A target.sample() result for these (level, ms) segments read `rate` times a second."""
    t, trans = 0, []
    for lvl, ms in levels:
        trans.append((t, lvl))
        t += ms
    return {"transitions": trans, "samples": int(t * rate / 1000), "span_ms": t, "output_enabled": 1}


class PatternEdit(unittest.TestCase):
    def test_first_edit_replaces_the_loop_c89_style(self):
        out = build_c28.with_pattern(TI_MAIN, [150, 150, 150, 900], "led_ex1_blinky.c")
        self.assertIn("static const uint32_t pattern_ms[] = {150, 150, 150, 900};", out)
        self.assertIn("    unsigned i;", out)
        self.assertNotIn("for(unsigned", out)
        self.assertNotIn("DEVICE_DELAY_US(500000)", out)

    def test_second_edit_changes_only_the_table(self):
        once = build_c28.with_pattern(TI_MAIN, [150, 150, 150, 900], "x.c")
        self.assertEqual(build_c28.with_pattern(once, [100, 700], "x.c"), once.replace("{150, 150, 150, 900};", "{100, 700};"))

    def test_hand_edited_source_is_refused(self):
        with self.assertRaises(Stop):
            build_c28.with_pattern(TI_MAIN.replace("500000", "250000"), [150, 150], "x.c")


class SlowProbe(unittest.TestCase):
    LEVELS = [(1, 160), (0, 140), (1, 150), (0, 910)] * 3

    def test_default_rule_refuses_a_slow_probe(self):
        self.assertEqual(verify.pattern_check(sample(self.LEVELS, 40), [150, 150, 150, 900], 40, "LED")["code"], "FRL-E042")

    def test_route_minimum_widens_the_tolerance_and_says_so(self):
        r = verify.pattern_check(sample(self.LEVELS, 40), [150, 150, 150, 900], 40, "LED", min_rate=20)
        self.assertTrue(r["ok"], r["message"])
        self.assertIn("tolerance 50 ms", r["message"])
        self.assertIn("two sample periods", r["data"]["method"])

    def test_below_the_route_minimum_is_still_unverified(self):
        self.assertEqual(verify.pattern_check(sample(self.LEVELS, 10), [150, 150, 150, 900], 40, "LED", min_rate=20)["code"], "FRL-E042")


if __name__ == "__main__":
    unittest.main()
