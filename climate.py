import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import climate_ir
from esphome.const import CONF_ID, CONF_SUPPORTS_HEAT

AUTO_LOAD = ["climate_ir"]
CODEOWNERS = ["@I_am_the_Carl"]

frigidaire_ns = cg.esphome_ns.namespace("frigidaire")
FrigidareClimate = frigidaire_ns.class_("FrigidareClimate", climate_ir.ClimateIR)

CONFIG_SCHEMA = climate_ir.CLIMATE_IR_WITH_RECEIVER_SCHEMA.extend(
    {
        cv.GenerateID(): cv.declare_id(FrigidareClimate),
        cv.Optional(CONF_SUPPORTS_HEAT, default=False): cv.boolean,
    }
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await climate_ir.register_climate_ir(var, config)
