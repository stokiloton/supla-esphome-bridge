import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import CONF_ID

supla_bridge_ns = cg.esphome_ns.namespace("supla_bridge")
SuplaBridge = supla_bridge_ns.class_("SuplaBridge", cg.Component)

CONF_SERVER = "server"
CONF_EMAIL = "user"
CONF_PASSWORD = "password"
CONF_SWITCH_ID = "switch_id"

CONFIG_SCHEMA = cv.Schema({
    cv.GenerateID(): cv.declare_id(SuplaBridge),
    cv.Required(CONF_SERVER): cv.string,
    cv.Required(CONF_EMAIL): cv.string,
    cv.Required(CONF_PASSWORD): cv.string,
    cv.Required(CONF_SWITCH_ID): cv.use_id(cg.Component),
})

async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    cg.add(var.set_server(config[CONF_SERVER]))
    cg.add(var.set_email(config[CONF_EMAIL]))
    cg.add(var.set_password(config[CONF_PASSWORD]))

    switch = await cg.get_variable(config[CONF_SWITCH_ID])
    cg.add(var.set_switch_callback(switch.make_call()))

    await cg.register_component(var, config)
