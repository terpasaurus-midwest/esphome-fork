import esphome.codegen as cg

CODEOWNERS = ["@dephekt"]
DEPENDENCIES = ["i2c", "ezo"]
MULTI_CONF = True

ezo_types_ns = cg.esphome_ns.namespace("ezo_types")
