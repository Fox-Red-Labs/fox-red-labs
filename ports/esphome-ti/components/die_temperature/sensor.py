import esphome.codegen as cg
from esphome.components import sensor
import esphome.config_validation as cv
from esphome.const import (
    DEVICE_CLASS_TEMPERATURE,
    ENTITY_CATEGORY_DIAGNOSTIC,
    STATE_CLASS_MEASUREMENT,
    UNIT_CELSIUS,
)
from esphome.types import ConfigType

die_temperature_ns = cg.esphome_ns.namespace("die_temperature")
DieTemperatureSensor = die_temperature_ns.class_("DieTemperatureSensor", sensor.Sensor, cg.PollingComponent)

CONFIG_SCHEMA = cv.All(
    sensor.sensor_schema(
        DieTemperatureSensor,
        unit_of_measurement=UNIT_CELSIUS,
        accuracy_decimals=1,
        device_class=DEVICE_CLASS_TEMPERATURE,
        state_class=STATE_CLASS_MEASUREMENT,
        entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
    ).extend(cv.polling_component_schema("60s")),
    cv.only_on(["cc27xx", "cc23xx"]),
)


async def to_code(config: ConfigType) -> None:
    var = await sensor.new_sensor(config)
    await cg.register_component(var, config)
