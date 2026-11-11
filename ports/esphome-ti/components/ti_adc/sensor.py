# SPDX-License-Identifier: MIT
"""Voltage on a DIO, read with TI's ADC driver (cc27xx, cc23xx). ESPHome's own `adc` sensor builds for ESP32, RP2 and nRF
only; this is the TI one. The pin must be an analog-capable DIO (see the pin table in the README)."""
import esphome.codegen as cg
from esphome import pins
from esphome.components import sensor
import esphome.config_validation as cv
from esphome.const import CONF_PIN, DEVICE_CLASS_VOLTAGE, STATE_CLASS_MEASUREMENT, UNIT_VOLT

ns = cg.esphome_ns.namespace("ti_adc")
TiAdcSensor = ns.class_("TiAdcSensor", sensor.Sensor, cg.PollingComponent)

CONFIG_SCHEMA = cv.All(
    sensor.sensor_schema(TiAdcSensor, unit_of_measurement=UNIT_VOLT, accuracy_decimals=3,
                         device_class=DEVICE_CLASS_VOLTAGE, state_class=STATE_CLASS_MEASUREMENT)
    .extend({cv.Required(CONF_PIN): pins.internal_gpio_input_pin_number})
    .extend(cv.polling_component_schema("60s")),
    cv.only_on(["cc27xx", "cc23xx"]),
)


async def to_code(config):
    var = await sensor.new_sensor(config)
    await cg.register_component(var, config)
    cg.add(var.set_pin(config[CONF_PIN]))
