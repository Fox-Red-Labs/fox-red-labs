"""The CC35X1 route's own pieces (tools/frl/toolbox.py, the uart2echo message edit, the reprogram backup), no board."""
import json
import sys
import tempfile
import unittest
from pathlib import Path
from unittest import mock

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools" / "frl"))
import ops  # noqa: E402
import target  # noqa: E402
import toolbox  # noqa: E402
from core import Stop, board_route  # noqa: E402

# The end of TI's uart2echo.c for LP_EM_CC35X1 (SimpleLink Wi-Fi SDK 10.20.00.39), shortened.
TI_SRC = """#include <stdint.h>
#include <ti/drivers/UART2.h>

/* Driver configuration */
#include "ti_drivers_config.h"

void *mainThread(void *arg0)
{
    char input;
    size_t bytesRead;
    size_t bytesWritten = 0;
    int_fast16_t status = UART2_STATUS_SUCCESS;
    UART2_write(uart, echoPrompt, sizeof(echoPrompt), &bytesWritten);

    /* Loop forever echoing */
    while (1)
    {
        status = UART2_read(uart, &input, 1, &bytesRead);
    }
}
"""

BOARD = board_route("cc35x1")


class MessageEdit(unittest.TestCase):
    def test_first_edit_prints_the_message_with_a_counter(self):
        new, line = ops.uart2echo_message(TI_SRC, "Hello CC35")
        self.assertEqual(line, 'static const char frlMessage[] = "Hello CC35";')
        self.assertIn(ops.MSG_MARK, new)
        self.assertIn('snprintf(frlLine, sizeof(frlLine), "> %s %u\\r\\n", frlMessage, loopCount++);', new)
        self.assertIn("#include <stdio.h>", new)
        self.assertNotIn("Loop forever echoing", new)

    def test_second_edit_changes_only_the_message(self):
        once, _ = ops.uart2echo_message(TI_SRC, "One")
        twice, _ = ops.uart2echo_message(once, "Two")
        self.assertEqual(twice, once.replace('"One"', '"Two"'))

    def test_hand_edited_source_is_refused(self):
        self.assertIsNone(ops.uart2echo_message(TI_SRC.replace("/* Loop forever echoing */", ""), "x")[0])


class Transport(unittest.TestCase):
    def test_settings_beside_the_image(self):
        with tempfile.TemporaryDirectory() as d:
            img = Path(d) / "uart2echo.out"
            img.write_bytes(b"x")
            with self.assertRaises(Stop):
                toolbox.settings_for(img)
            (Path(d) / "toolbox").mkdir()
            (Path(d) / "toolbox" / "tool_settings.json").write_text("{}")
            self.assertEqual(toolbox.settings_for(img), Path(d) / "toolbox" / "tool_settings.json")

    def test_no_read_back_is_unknown_not_a_pass(self):
        same, why = target.verify_image(BOARD, "E10000FQ", "x.out")
        self.assertIsNone(same)
        self.assertIn("no read-back", why)

    def test_write_report_counts_bytes_and_names_no_verify(self):
        r = target.write_report("Writing binary size of 12 bytes within 0.2 sec\nWriting binary size of 1291256 bytes within 20.6 sec")
        self.assertEqual(r["bytes"], 1291268)
        self.assertTrue(r["verify_path"].startswith("none"))

    def test_identity_checks_the_part(self):
        with mock.patch("discover.identify", return_value={"part": ["CC2340R5"], "how": "x"}):
            with self.assertRaises(Stop) as e:
                toolbox.check_identity(BOARD, "E10000FQ")
        self.assertEqual(e.exception.result["outcome"], "refused")
        with mock.patch("discover.identify", return_value={"part": ["CC3551E"], "how": "SWD, configuration port DEVICEID 0x1BB9602F"}), \
             mock.patch.object(toolbox, "udid", return_value="0123456789ABCDEF"):
            self.assertEqual(toolbox.check_identity(BOARD, "E10000FQ")["part"], "CC3551E")

    def test_programming_without_a_written_line_fails(self):
        with tempfile.TemporaryDirectory() as d:
            (Path(d) / "toolbox").mkdir()
            (Path(d) / "toolbox" / "tool_settings.json").write_text("{}")
            with mock.patch.object(toolbox, "run", return_value=(0, "Error: -615 connect failed")), \
                 mock.patch.object(toolbox, "toolbox", return_value={"exe": "x", "path": d}):
                with self.assertRaises(Stop) as e:
                    toolbox.program(BOARD, "E10000FQ", str(Path(d) / "toolbox" / "tool_settings.json"))
        self.assertEqual(e.exception.result["code"], "FRL-E006")


if __name__ == "__main__":
    unittest.main()
