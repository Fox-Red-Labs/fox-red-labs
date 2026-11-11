# SPDX-License-Identifier: MIT
"""PWM output on a DIO through TI's PWM driver and an LGPT timer (cc27xx, cc23xx). Use it with `output:`, then
`light: monochromatic` or `fan: speed`. Each PWM pin takes one of the chip's LGPT timers."""
import esphome.codegen as cg
from esphome import pins
from esphome.components import output
import esphome.config_validation as cv
from esphome.const import CONF_FREQUENCY, CONF_ID, CONF_PIN

ns = cg.esphome_ns.namespace("ti_pwm")
TiPwmOutput = ns.class_("TiPwmOutput", output.FloatOutput, cg.Component)

CONFIG_SCHEMA = cv.All(
    output.FLOAT_OUTPUT_SCHEMA.extend(
        {
            cv.Required(CONF_ID): cv.declare_id(TiPwmOutput),
            cv.Required(CONF_PIN): pins.internal_gpio_output_pin_number,
            cv.Optional(CONF_FREQUENCY, default="1kHz"): cv.All(cv.frequency, cv.int_range(min=20, max=1000000)),
        }
    ).extend(cv.COMPONENT_SCHEMA),
    cv.only_on(["cc27xx", "cc23xx"]),
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await output.register_output(var, config)
    cg.add(var.set_pin(config[CONF_PIN]))
    cg.add(var.set_frequency(int(config[CONF_FREQUENCY])))
