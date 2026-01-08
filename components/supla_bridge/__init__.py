import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import CONF_ID

from esphome import automation

supla_bridge_ns = cg.esphome_ns.namespace("supla_bridge")
SuplaBridge = supla_bridge_ns.class_("SuplaBridge", cg.Component)

# Nowe klasy triggerów
SuplaTurnOnTrigger = supla_bridge_ns.class_("SuplaTurnOnTrigger", automation.Trigger.template())
SuplaTurnOffTrigger = supla_bridge_ns.class_("SuplaTurnOffTrigger", automation.Trigger.template())

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

    # Trigger: SUPLA -> ESPHome
    if CONF_ON_SUPLA_TURN_ON in config:
        trig = cg.new_Pvariable(SuplaTurnOnTrigger)
        cg.add(var.set_on_turn_on_trigger(trig))
        await automation.build_automation(
            trig,
            [],
            config[CONF_ON_SUPLA_TURN_ON],
        )

    if CONF_ON_SUPLA_TURN_OFF in config:
        trig = cg.new_Pvariable(SuplaTurnOffTrigger)
        cg.add(var.set_on_turn_off_trigger(trig))
        await automation.build_automation(
            trig,
            [],
            config[CONF_ON_SUPLA_TURN_OFF],
        )
