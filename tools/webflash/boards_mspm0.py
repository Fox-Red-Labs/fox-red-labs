"""Per-board facts for the browser bench's MSPM0 boards other than the LP-MSPM0L1306.

One owner per fact. Every value below is from TI's MSPM0 SDK 2.11.00.07 (paths in SOURCES) or was measured on
the board (the draft JSON in site/flash/bench/boards-draft/ carries those results). Used by build_board.py.
"""
SDK = "/Applications/ti/mspm0_sdk_2_11_00_07"
GPIOA, GPIOB = 0x400A0000, 0x400A2000

BOARDS = {
    "mspm0g3507": {
        "kit": "LP-MSPM0G3507", "serial": "MG350001", "example": "examples/nortos/LP_MSPM0G3507/driverlib/gpio_toggle_output",
        # SLASEX6C tables 8-21/8-22: family ID is shared with G3505/G3506; USERID distinguishes the G3507.
        "deviceid": 0x0BB8802F, "userid": 0xAE2D, "userid_mask": 0xFFFF,
        "flash_end": 0x20000, "table": 0x1FC00,
        "channels": {"red": ("GPIOB", 26, GPIOB, "LED2 red (PB26)"), "green": ("GPIOB", 27, GPIOB, "LED2 green (PB27)"),
                     "blue": ("GPIOB", 22, GPIOB, "LED2 blue (PB22)")},
        "active_high": True,
    },
    "mspm0c1104": {
        "kit": "LP-MSPM0C1104", "serial": "MC110001", "example": "examples/nortos/LP_MSPM0C1104/driverlib/gpio_toggle_output",
        "deviceid": 0x3BBA102F, "flash_end": 0x4000, "table": 0x3C00,
        # channel: (port symbol, bit, gpio base, label). The board has one LED, wired to the red channel.
        "channels": {"red": ("GPIOA", 22, GPIOA, "LED1 red (PA22)"), "green": None, "blue": None},
        "active_high": False,
        "button": ("GPIOA", 16, GPIOA, "IOMUX_PINCM17", "S2 (PA16)"),   # S2 to ground, no external pull (SDK gpio_software_poll README)
    },
    "mspm0g3519": {
        "kit": "LP-MSPM0G3519", "serial": "MG000001", "example": "examples/nortos/LP_MSPM0G3519/driverlib/gpio_toggle_output",
        "deviceid": 0x0BBA902F, "flash_end": 0x80000, "table": 0x7FC00,
        "channels": {"red": ("GPIOB", 26, GPIOB, "LED2 red (PB26)"), "green": ("GPIOB", 27, GPIOB, "LED2 green (PB27)"),
                     "blue": ("GPIOB", 22, GPIOB, "LED2 blue (PB22)")},
        "active_high": True,
        "button": ("GPIOB", 3, GPIOB, "IOMUX_PINCM16", "S2 (PB3)"),   # SDK gpio_software_poll README: PB3, internal pull-up
    },
    "mspm0g5187": {
        "kit": "LP-MSPM0G5187", "serial": "MG020001", "example": "examples/nortos/LP_MSPM0G5187/driverlib/gpio_toggle_output",
        "deviceid": 0x0BBBC02F, "flash_end": 0x20000, "table": 0x1FC00,
        "channels": {"red": ("GPIOA", 17, GPIOA, "LED2 red (PA17)"), "green": ("GPIOA", 24, GPIOA, "LED2 green (PA24)"),
                     "blue": ("GPIOB", 13, GPIOB, "LED2 blue (PB13)")},
        "active_high": True,
    },
}
