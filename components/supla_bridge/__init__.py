import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import sensor, light
from esphome.const import CONF_ID

CONF_SERVER = "server"
CONF_LOCATION_ID = "location_id"
CONF_LOCATION_PASSWORD = "location_password"
CONF_DEVICE_NAME = "device_name"
CONF_TEMPERATURE = "temperature"
CONF_SWITCH = "switch"

supla_ns = cg.esphome_ns.namespace("supla_esphome_bridge")
SuplaEsphomeBridge = supla_ns.class_("SuplaEsphomeBridge", cg.Component)

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(SuplaEsphomeBridge),
        cv.Required(CONF_SERVER): cv.string,
        cv.Required(CONF_LOCATION_ID): cv.int_,
        cv.Required(CONF_LOCATION_PASSWORD): cv.string,
        cv.Optional(CONF_DEVICE_NAME, default="esphome"): cv.string,
        cv.Required(CONF_TEMPERATURE): cv.use_id(sensor.Sensor),
        cv.Required(CONF_SWITCH): cv.use_id(light.LightState),
    }
)

async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)

    cg.add(var.set_server(config[CONF_SERVER]))
    cg.add(var.set_location_id(config[CONF_LOCATION_ID]))
    cg.add(var.set_location_password(config[CONF_LOCATION_PASSWORD]))
    cg.add(var.set_device_name(config[CONF_DEVICE_NAME]))

    temp = await cg.get_variable(config[CONF_TEMPERATURE])
    sw = await cg.get_variable(config[CONF_SWITCH])

    cg.add(var.set_temperature_sensor(temp))
    cg.add(var.set_switch_light(sw))
