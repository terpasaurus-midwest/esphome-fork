import esphome.codegen as cg
from esphome.components import binary_sensor, sensor
import esphome.config_validation as cv
from esphome.const import CONF_ID

m5cores3_power_ns = cg.esphome_ns.namespace("m5cores3_power")
M5CoreS3Power = m5cores3_power_ns.class_("M5CoreS3Power", cg.Component)

CONF_BATTERY_SENSOR = "battery_sensor"
CONF_BATTERY_PRESENT_SENSOR = "battery_present_sensor"
CONF_BATTERY_CHARGING_SENSOR = "battery_charging_sensor"

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(M5CoreS3Power),
        cv.Optional(CONF_BATTERY_SENSOR): sensor.sensor_schema(
            unit_of_measurement="%",
            accuracy_decimals=0,
            device_class="battery",
            state_class="measurement",
        ),
        cv.Optional(CONF_BATTERY_PRESENT_SENSOR): binary_sensor.binary_sensor_schema(
            device_class="battery",
        ),
        cv.Optional(CONF_BATTERY_CHARGING_SENSOR): binary_sensor.binary_sensor_schema(
            device_class="battery_charging",
        ),
    }
).extend(cv.COMPONENT_SCHEMA)


def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    yield cg.register_component(var, config)

    if CONF_BATTERY_SENSOR in config:
        sens = yield sensor.new_sensor(config[CONF_BATTERY_SENSOR])
        cg.add(var.set_battery_sensor(sens))

    if CONF_BATTERY_PRESENT_SENSOR in config:
        sens = yield binary_sensor.new_binary_sensor(
            config[CONF_BATTERY_PRESENT_SENSOR]
        )
        cg.add(var.set_battery_present_sensor(sens))

    if CONF_BATTERY_CHARGING_SENSOR in config:
        sens = yield binary_sensor.new_binary_sensor(
            config[CONF_BATTERY_CHARGING_SENSOR]
        )
        cg.add(var.set_battery_charging_sensor(sens))


# Import automations to register actions after classes are defined
from . import automations  # noqa
