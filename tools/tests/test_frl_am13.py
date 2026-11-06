"""The LP-AM13E230 route's own pieces (tools/frl/build_am13.py), with no SDK and no board."""
import sys
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools" / "frl"))
import build_am13  # noqa: E402
from core import Stop  # noqa: E402

# The main() of TI's gpio_toggle_output for the AM13E230x LaunchPad (AM13E230x SDK 26.01.00.03), as shipped.
TI_MAIN = """#include "ti_sdk_dl_config.h"

int main()
{
    Device_Init();
    SYSCFG_DL_init();

    while (1)
    {
        DL_GPIO_togglePins(GPIO_GRP_0_PORT, GPIO_GRP_0_PIN_0_PIN);

        /* 500ms delay after toggle */
        DEVICE_DELAY_MS(500);
    }

    return 0;
}
"""


class PatternEdit(unittest.TestCase):
    def test_first_edit_replaces_the_toggle_loop(self):
        out = build_am13.with_pattern(TI_MAIN, [150, 150, 150, 900], "main.c")
        self.assertIn(build_am13.MARK, out)
        self.assertIn("static const uint32_t pattern_ms[] = {150, 150, 150, 900};", out)
        self.assertIn("DEVICE_DELAY_MS(pattern_ms[i]);", out)
        self.assertNotIn("DL_GPIO_togglePins", out)
        self.assertEqual(out.count("int main()"), 1)

    def test_second_edit_changes_only_the_table(self):
        once = build_am13.with_pattern(TI_MAIN, [150, 150, 150, 900], "main.c")
        twice = build_am13.with_pattern(once, [100, 700], "main.c")
        self.assertIn("{100, 700};", twice)
        self.assertEqual(once.replace("{150, 150, 150, 900};", "{100, 700};"), twice)

    def test_hand_edited_source_is_refused(self):
        with self.assertRaises(Stop):
            build_am13.with_pattern(TI_MAIN.replace("DEVICE_DELAY_MS(500);", "DEVICE_DELAY_MS(250);"), [150, 150], "main.c")


if __name__ == "__main__":
    unittest.main()
