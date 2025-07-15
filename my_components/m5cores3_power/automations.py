from esphome import automation
import esphome.codegen as cg
import esphome.config_validation as cv

from . import M5CoreS3Power, m5cores3_power_ns

LogAxp2101DiagnosticsAction = m5cores3_power_ns.class_(
    "LogAxp2101DiagnosticsAction", automation.Action
)

LOG_AXP2101_DIAGNOSTICS_ACTION_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(LogAxp2101DiagnosticsAction),
        cv.Required("component_id"): cv.use_id(M5CoreS3Power),
    }
)


@automation.register_action(
    "m5cores3_power.log_axp2101_diagnostics",
    LogAxp2101DiagnosticsAction,
    LOG_AXP2101_DIAGNOSTICS_ACTION_SCHEMA,
)
def log_axp2101_diagnostics_action_to_code(config, action_id, template_arg, args):
    var = cg.new_Pvariable(action_id, template_arg)
    component = yield cg.get_variable(config["component_id"])
    cg.add(var.set_component(component))
    yield var
