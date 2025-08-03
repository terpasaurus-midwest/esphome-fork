import esphome.codegen as cg
from esphome.components import i2c, sensor, text_sensor
from esphome.components.ezo import EZOSensor
import esphome.config_validation as cv
from esphome.const import (
    CONF_ID,
    CONF_VOLTAGE,
    DEVICE_CLASS_VOLTAGE,
    STATE_CLASS_MEASUREMENT,
    UNIT_VOLT,
)

CODEOWNERS = ["@dephekt"]
DEPENDENCIES = ["i2c", "ezo"]

CONF_TYPE = "type"
CONF_RESET_REASON = "reset_reason"
CONF_FIRMWARE_VERSION = "firmware_version"
CONF_TDS = "tds"
CONF_SALINITY = "salinity"
CONF_RELATIVE_DENSITY = "relative_density"


ezo_types_ns = cg.esphome_ns.namespace("ezo_types")

PHSensor = ezo_types_ns.class_("PHSensor", EZOSensor)
ECSensor = ezo_types_ns.class_("ECSensor", EZOSensor)
RTDSensor = ezo_types_ns.class_("RTDSensor", EZOSensor)
ORPSensor = ezo_types_ns.class_("ORPSensor", EZOSensor)


def _voltage_sensor_schema():
    return sensor.sensor_schema(
        device_class=DEVICE_CLASS_VOLTAGE,
        state_class=STATE_CLASS_MEASUREMENT,
        unit_of_measurement=UNIT_VOLT,
        accuracy_decimals=3,
    )


def _reset_reason_schema():
    return text_sensor.text_sensor_schema()


def _firmware_version_schema():
    return text_sensor.text_sensor_schema()


CONFIG_SCHEMA = cv.typed_schema(
    {
        "ph": sensor.sensor_schema(PHSensor)
        .extend(
            {
                cv.Optional(CONF_VOLTAGE): _voltage_sensor_schema(),
                cv.Optional(CONF_RESET_REASON): _reset_reason_schema(),
                cv.Optional(CONF_FIRMWARE_VERSION): _firmware_version_schema(),
            }
        )
        .extend(cv.polling_component_schema("60s"))
        .extend(i2c.i2c_device_schema(None)),
        "ec": sensor.sensor_schema(ECSensor)
        .extend(
            {
                cv.Optional(CONF_VOLTAGE): _voltage_sensor_schema(),
                cv.Optional(CONF_RESET_REASON): _reset_reason_schema(),
                cv.Optional(CONF_FIRMWARE_VERSION): _firmware_version_schema(),
                cv.Optional(CONF_TDS): sensor.sensor_schema(
                    unit_of_measurement="ppm",
                    accuracy_decimals=0,
                    state_class=STATE_CLASS_MEASUREMENT,
                ),
                cv.Optional(CONF_SALINITY): sensor.sensor_schema(
                    unit_of_measurement="ppt",
                    accuracy_decimals=2,
                    state_class=STATE_CLASS_MEASUREMENT,
                ),
                cv.Optional(CONF_RELATIVE_DENSITY): sensor.sensor_schema(
                    accuracy_decimals=3,
                    state_class=STATE_CLASS_MEASUREMENT,
                ),
            }
        )
        .extend(cv.polling_component_schema("60s"))
        .extend(i2c.i2c_device_schema(None)),
        "rtd": sensor.sensor_schema(RTDSensor)
        .extend(
            {
                cv.Optional(CONF_VOLTAGE): _voltage_sensor_schema(),
                cv.Optional(CONF_RESET_REASON): _reset_reason_schema(),
                cv.Optional(CONF_FIRMWARE_VERSION): _firmware_version_schema(),
            }
        )
        .extend(cv.polling_component_schema("60s"))
        .extend(i2c.i2c_device_schema(None)),
        "orp": sensor.sensor_schema(ORPSensor)
        .extend(
            {
                cv.Optional(CONF_VOLTAGE): _voltage_sensor_schema(),
                cv.Optional(CONF_RESET_REASON): _reset_reason_schema(),
                cv.Optional(CONF_FIRMWARE_VERSION): _firmware_version_schema(),
            }
        )
        .extend(cv.polling_component_schema("60s"))
        .extend(i2c.i2c_device_schema(None)),
    },
    key=CONF_TYPE,
)


async def setup_atlas_sensor_base(var, config):
    """Common setup for all Atlas EZO sensors"""
    await cg.register_component(var, config)
    await sensor.register_sensor(var, config)
    await i2c.register_i2c_device(var, config)

    if voltage_config := config.get(CONF_VOLTAGE):
        voltage_sensor = await sensor.new_sensor(voltage_config)
        cg.add(var.set_voltage_sensor(voltage_sensor))

    if reset_reason_config := config.get(CONF_RESET_REASON):
        reset_reason_sensor = await text_sensor.new_text_sensor(reset_reason_config)
        cg.add(var.set_reset_reason_sensor(reset_reason_sensor))

    if firmware_version_config := config.get(CONF_FIRMWARE_VERSION):
        firmware_version_sensor = await text_sensor.new_text_sensor(
            firmware_version_config
        )
        cg.add(var.set_firmware_version_sensor(firmware_version_sensor))


async def to_code(config):
    sensor_type = config[CONF_TYPE]
    var = cg.new_Pvariable(config[CONF_ID])
    await setup_atlas_sensor_base(var, config)

    if sensor_type == "ph":
        pass

    elif sensor_type == "ec":
        if tds_config := config.get(CONF_TDS):
            tds_sensor = await sensor.new_sensor(tds_config)
            cg.add(var.set_tds_sensor(tds_sensor))

        if salinity_config := config.get(CONF_SALINITY):
            salinity_sensor = await sensor.new_sensor(salinity_config)
            cg.add(var.set_salinity_sensor(salinity_sensor))

        if relative_density_config := config.get(CONF_RELATIVE_DENSITY):
            relative_density_sensor = await sensor.new_sensor(relative_density_config)
            cg.add(var.set_relative_density_sensor(relative_density_sensor))

    elif sensor_type == "rtd":
        pass

    elif sensor_type == "orp":
        pass
