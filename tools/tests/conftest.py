"""pytest setup for tools/tests.

Eight tests are scripts: they run their checks at import time and end in sys.exit(), so pytest cannot import them.
conftest keeps pytest from importing them and test_script_tests.py runs each one as `python3 <file>` instead.
"""
SCRIPT_TESTS = [
    "test_hooks_security.py",
    "test_motor_plots.py",
    "test_power_agree.py",
    "test_power_loop.py",
    "test_power_pwl.py",
    "test_power_verdict.py",
    "test_power_loss.py",
    "test_power_stage.py",
]
collect_ignore = list(SCRIPT_TESTS)
