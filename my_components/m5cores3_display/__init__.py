import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import CONF_ID

DEPENDENCIES = ["esp32"]

CONF_CO2L_COMPONENT = "co2l_component"

m5cores3_display_ns = cg.esphome_ns.namespace("m5cores3_display")
M5CoreS3Display = m5cores3_display_ns.class_("M5CoreS3Display", cg.Component)

# Import the M5Unit CO2L component
m5unit_co2l_ns = cg.esphome_ns.namespace("m5unit_co2l")
M5UnitCO2L = m5unit_co2l_ns.class_("M5UnitCO2L")

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(M5CoreS3Display),
        cv.Optional(CONF_CO2L_COMPONENT): cv.use_id(M5UnitCO2L),
    }
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)

    if CONF_CO2L_COMPONENT in config:
        co2l_component = await cg.get_variable(config[CONF_CO2L_COMPONENT])
        cg.add(var.set_co2l_component(co2l_component))
