"""The shared parameter block of the browser bench's images (format 1): one source for what each image lets you change.

Every image that has a Change tab holds its settings as 32-bit words in the board's last flash sector (the same
sector the color player's table uses; boards_mspm0.py "table"). The page rewrites only that sector; no compiler
runs. Layout, little-endian words:

    word 0   magic 0x50524C46 ("FLRP")
    word 1   format version, 1
    word 2   count of values that follow
    word 3   image id (a number per image kind, below): a block written for another image is ignored
    word 4.. one unsigned value per parameter, in the order of the list below

An image that finds the magic, version and id right, and a value inside [min, max], uses it; anything else gives the
default. The same list goes into the image's manifest ("params"), where the page reads labels, units and limits.
build_board.py writes frl_params_gen.h from this file, and player/frl_params.h reads the block, so the numbers
exist once. The color player keeps its own sequence table (magic "FLCR"); the manifest says which kind an image has.
Documented for readers and for the next image in site/flash/bench/README.md.
"""
MAGIC = 0x50524C46
FORMAT = 1


def p(pid, label, unit, lo, hi, default, step=1, note=""):
    return {"id": pid, "label": label, "unit": unit, "min": lo, "max": hi, "default": default, "step": step, "note": note}


# image kind -> (image id, [parameters])
KINDS = {
    "breathe": (2, [p("period_ms", "Time for one breath", "ms", 400, 10000, 2000, 100, "up and back down, the whole cycle")]),
    "thermometer": (3, [p("sample_ms", "Time between readings", "ms", 20, 2000, 100, 10, "how often the ADC reads the chip's temperature"),
                        p("blink_every", "LED flash every", "readings", 1, 100, 10, 1, "the red LED shows the program is running")]),
    "digit": (4, [p("led_ms", "LED flash per answer", "ms", 0, 1000, 80, 10, "0 turns the flash off")]),
    "occ": (5, [p("led_ms", "LED flash per answer", "ms", 0, 1000, 80, 10, "0 turns the flash off")]),
    # the radio demos (cc2755p10/radio-demos/frl_demo.c, cc2340r5): same block format, written by the page as a whole image write (cc-saci.js imageWithParams)
    "radio_blink": (6, [p("on_ms", "LED on", "ms", 10, 60000, 500, 10), p("off_ms", "LED off", "ms", 10, 60000, 500, 10)]),
    "radio_message": (7, [p("period_ms", "Time between lines", "ms", 50, 60000, 1000, 50, "the line counts up each time")]),
    "radio_button": (8, [p("repeat_ms", "Repeat the buttons line every", "ms", 100, 60000, 1000, 100, "lines also print at once on every press or release")]),
}


def blink_kind(pattern):
    """The blink image's values are its pattern: on, off, on, off, ... in ms (up to 8 values; longer patterns, like SOS, have no block)."""
    if not 2 <= len(pattern) <= 8:
        return None
    return (1, [p(f"{'on' if i % 2 == 0 else 'off'}{i // 2 + 1}", f"LED {'on' if i % 2 == 0 else 'off'}, part {i // 2 + 1}", "ms", 10, 5000, v, 10) for i, v in enumerate(pattern)])


def manifest_block(kind, table_addr, pattern=None):
    k = blink_kind(pattern) if kind == "blink" else KINDS.get(kind)
    if not k:
        return None
    image_id, items = k
    return {"kind": "params", "format": FORMAT, "addr": f"0x{table_addr:08X}", "magic": f"0x{MAGIC:08X}", "image_id": image_id, "items": items}


def gen_header(kind, pattern=None):
    k = blink_kind(pattern) if kind == "blink" else KINDS.get(kind)
    lines = ["/* Written by tools/webflash/build_board.py from tools/webflash/params.py. Do not edit. */",
             f"#define FRL_PARAMS_MAGIC ({MAGIC:#x}u)", f"#define FRL_PARAMS_FORMAT ({FORMAT}u)"]
    if not k:
        return "\n".join(lines + ["#define FRL_IMAGE_ID (0u)", "#define FRL_N_PARAMS (0u)"]) + "\n"
    image_id, items = k
    lines += [f"#define FRL_IMAGE_ID ({image_id}u)", f"#define FRL_N_PARAMS ({len(items)}u)",
              "static const uint32_t frl_def[] = {" + ", ".join(f"{i['default']}u" for i in items) + "};",
              "static const uint32_t frl_lo[] = {" + ", ".join(f"{i['min']}u" for i in items) + "};",
              "static const uint32_t frl_hi[] = {" + ", ".join(f"{i['max']}u" for i in items) + "};"]
    return "\n".join(lines) + "\n"
