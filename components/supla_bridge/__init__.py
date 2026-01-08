import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import CONF_ID

from esphome import automation

supla_bridge_ns = cg.esphome_ns.namespace("supla_bridge")
SuplaBridge = supla_bridge_ns.class_("SuplaBridge", cg.Component)

CONF_SERVER = "server"
CONF_USER = "user"
CONF_PASSWORD = "password"

CONF_ON_SUPLA_TURN_ON = "on_supla_turn_on"
CONF_ON_SUPLA_TURN_OFF = "on_supla_turn_off"

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(SuplaBridge),
        cv.Required(CONF_SERVER): cv.string,
        cv.Required(CONF_USER): cv.string,
        cv.Required(CONF_PASSWORD): cv.string,

        cv.Optional(CONF_ON_SUPLA_TURN_ON): automation.validate_automation(
            cv.Schema({})
        ),
        cv.Optional(CONF_ON_SUPLA_TURN_OFF): automation.validate_automation(
            cv.Schema({})
        ),
    }
).extend(cv.COMPONENT_SCHEMA)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)

    cg.add(var.set_server(config[CONF_SERVER]))
    cg.add(var.set_email(config[CONF_USER]))
    cg.add(var.set_password(config[CONF_PASSWORD]))

    # Triggery: SUPLA -> ESPHome
    if CONF_ON_SUPLA_TURN_ON in config:
        on_turn_on_trigger = cg.new_Pvariable(
            cg.global_ns.class_("Trigger")()
        )
        cg.add(var.set_on_turn_on_trigger(on_turn_on_trigger))
        await automation.build_automation(
            on_turn_on_trigger,
            [],
            config[CONF_ON_SUPLA_TURN_ON],
        )

    if CONF_ON_SUPLA_TURN_OFF in config:
        on_turn_off_trigger = cg.new_Pvariable(
            cg.global_ns.class_("Trigger")()
        )
        cg.add(var.set_on_turn_off_trigger(on_turn_off_trigger))
        await automation.build_automation(
            on_turn_off_trigger,
            [],
            config[CONF_ON_SUPLA_TURN_OFF],
        )
