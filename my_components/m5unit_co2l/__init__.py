from esphome import automation
import esphome.codegen as cg
from esphome.components import sensor
import esphome.config_validation as cv
from esphome.const import (
    CONF_HUMIDITY,
    CONF_ID,
    CONF_TEMPERATURE,
    DEVICE_CLASS_CARBON_DIOXIDE,
    DEVICE_CLASS_HUMIDITY,
    DEVICE_CLASS_TEMPERATURE,
    STATE_CLASS_MEASUREMENT,
    UNIT_CELSIUS,
    UNIT_PARTS_PER_MILLION,
    UNIT_PERCENT,
)

DEPENDENCIES = ["esp32"]
AUTO_LOAD = ["sensor"]

# Configuration keys
CONF_CO2 = "co2"
CONF_VPD = "vpd"

# Units
UNIT_KILOPASCAL = "kPa"

# Device classes
DEVICE_CLASS_PRESSURE = "pressure"

m5unit_co2l_ns = cg.esphome_ns.namespace("m5unit_co2l")
M5UnitCO2L = m5unit_co2l_ns.class_("M5UnitCO2L", cg.Component)

# Actions
M5UnitCO2LPerformForcedCalibrationAction = m5unit_co2l_ns.class_(
    "M5UnitCO2LPerformForcedCalibrationAction", automation.Action
)
M5UnitCO2LFactoryResetAction = m5unit_co2l_ns.class_(
    "M5UnitCO2LFactoryResetAction", automation.Action
)
M5UnitCO2LSetTemperatureOffsetAction = m5unit_co2l_ns.class_(
    "M5UnitCO2LSetTemperatureOffsetAction", automation.Action
)
M5UnitCO2LSetAutomaticSelfCalibrationAction = m5unit_co2l_ns.class_(
    "M5UnitCO2LSetAutomaticSelfCalibrationAction", automation.Action
)

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(M5UnitCO2L),
        cv.Optional(CONF_CO2): sensor.sensor_schema(
            unit_of_measurement=UNIT_PARTS_PER_MILLION,
            accuracy_decimals=0,
            device_class=DEVICE_CLASS_CARBON_DIOXIDE,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
        cv.Optional(CONF_TEMPERATURE): sensor.sensor_schema(
            unit_of_measurement=UNIT_CELSIUS,
            accuracy_decimals=2,
            device_class=DEVICE_CLASS_TEMPERATURE,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
        cv.Optional(CONF_HUMIDITY): sensor.sensor_schema(
            unit_of_measurement=UNIT_PERCENT,
            accuracy_decimals=2,
            device_class=DEVICE_CLASS_HUMIDITY,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
        cv.Optional(CONF_VPD): sensor.sensor_schema(
            unit_of_measurement=UNIT_KILOPASCAL,
            accuracy_decimals=2,
            device_class=DEVICE_CLASS_PRESSURE,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
    }
)

# Action schemas
PERFORM_FORCED_CALIBRATION_ACTION_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.use_id(M5UnitCO2L),
        cv.Required("target_ppm"): cv.templatable(cv.uint16_t),
    }
)

FACTORY_RESET_ACTION_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.use_id(M5UnitCO2L),
    }
)

SET_TEMPERATURE_OFFSET_ACTION_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.use_id(M5UnitCO2L),
        cv.Required("offset"): cv.templatable(cv.float_),
    }
)

SET_AUTOMATIC_SELF_CALIBRATION_ACTION_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.use_id(M5UnitCO2L),
        cv.Required("enabled"): cv.templatable(cv.boolean),
    }
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)

    # Add required libraries
    cg.add_library("m5stack/M5UnitUnified", None)
    cg.add_library("m5stack/M5Unit-ENV", None)

    # Register sensors
    if CONF_CO2 in config:
        sens = await sensor.new_sensor(config[CONF_CO2])
        cg.add(var.set_co2_sensor(sens))

    if CONF_TEMPERATURE in config:
        sens = await sensor.new_sensor(config[CONF_TEMPERATURE])
        cg.add(var.set_temperature_sensor(sens))

    if CONF_HUMIDITY in config:
        sens = await sensor.new_sensor(config[CONF_HUMIDITY])
        cg.add(var.set_humidity_sensor(sens))

    if CONF_VPD in config:
        sens = await sensor.new_sensor(config[CONF_VPD])
        cg.add(var.set_vpd_sensor(sens))


# Register actions
@automation.register_action(
    "m5unit_co2l.perform_forced_calibration",
    M5UnitCO2LPerformForcedCalibrationAction,
    PERFORM_FORCED_CALIBRATION_ACTION_SCHEMA,
)
async def perform_forced_calibration_action_to_code(
    config, action_id, template_arg, args
):
    parent = await cg.get_variable(config[CONF_ID])
    var = cg.new_Pvariable(action_id, template_arg, parent)
    template_ = await cg.templatable(config["target_ppm"], args, cg.uint16)
    cg.add(var.set_target_ppm(template_))
    return var


@automation.register_action(
    "m5unit_co2l.factory_reset",
    M5UnitCO2LFactoryResetAction,
    FACTORY_RESET_ACTION_SCHEMA,
)
async def factory_reset_action_to_code(config, action_id, template_arg, args):
    parent = await cg.get_variable(config[CONF_ID])
    var = cg.new_Pvariable(action_id, template_arg, parent)
    return var


@automation.register_action(
    "m5unit_co2l.set_temperature_offset",
    M5UnitCO2LSetTemperatureOffsetAction,
    SET_TEMPERATURE_OFFSET_ACTION_SCHEMA,
)
async def set_temperature_offset_action_to_code(config, action_id, template_arg, args):
    parent = await cg.get_variable(config[CONF_ID])
    var = cg.new_Pvariable(action_id, template_arg, parent)
    template_ = await cg.templatable(config["offset"], args, cg.float_)
    cg.add(var.set_offset(template_))
    return var


@automation.register_action(
    "m5unit_co2l.set_automatic_self_calibration",
    M5UnitCO2LSetAutomaticSelfCalibrationAction,
    SET_AUTOMATIC_SELF_CALIBRATION_ACTION_SCHEMA,
)
async def set_automatic_self_calibration_action_to_code(
    config, action_id, template_arg, args
):
    parent = await cg.get_variable(config[CONF_ID])
    var = cg.new_Pvariable(action_id, template_arg, parent)
    template_ = await cg.templatable(config["enabled"], args, cg.bool_)
    cg.add(var.set_enabled(template_))
    return var
