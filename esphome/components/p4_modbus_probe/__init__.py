import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import sensor, uart
from esphome.const import CONF_ID

CODEOWNERS = ["@hermes-agent"]
DEPENDENCIES = ["uart"]
DOMAIN = "p4_modbus_probe"

CONF_FRAME_TIMEOUT_MS = "frame_timeout_ms"
CONF_MAX_FRAME_SIZE = "max_frame_size"
CONF_SLAVE_ADDRESS = "slave_address"
CONF_REGISTER_COUNT = "register_count"
CONF_STATS_INTERVAL = "stats_interval"
CONF_REQUEST_COUNT = "request_count"
CONF_RESPONSE_COUNT = "response_count"
CONF_CRC_ERROR_COUNT = "crc_error_count"

p4_modbus_probe_ns = cg.esphome_ns.namespace(DOMAIN)
P4ModbusProbe = p4_modbus_probe_ns.class_("P4ModbusProbe", cg.Component, uart.UARTDevice)

CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(P4ModbusProbe),
            cv.Optional(CONF_FRAME_TIMEOUT_MS, default=5): cv.int_range(min=1, max=1000),
            cv.Optional(CONF_MAX_FRAME_SIZE, default=256): cv.int_range(min=16, max=1024),
            cv.Optional(CONF_SLAVE_ADDRESS, default=2): cv.int_range(min=1, max=247),
            cv.Optional(CONF_REGISTER_COUNT, default=400): cv.int_range(min=1, max=4096),
            cv.Optional(CONF_STATS_INTERVAL, default="60s"): cv.positive_time_period_milliseconds,
            cv.Optional(CONF_REQUEST_COUNT): sensor.sensor_schema(
                accuracy_decimals=0,
                icon="mdi:arrow-down-bold",
                entity_category="diagnostic",
            ),
            cv.Optional(CONF_RESPONSE_COUNT): sensor.sensor_schema(
                accuracy_decimals=0,
                icon="mdi:arrow-up-bold",
                entity_category="diagnostic",
            ),
            cv.Optional(CONF_CRC_ERROR_COUNT): sensor.sensor_schema(
                accuracy_decimals=0,
                icon="mdi:close-network",
                entity_category="diagnostic",
            ),
        }
    )
    .extend(cv.COMPONENT_SCHEMA)
    .extend(uart.UART_DEVICE_SCHEMA)
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await uart.register_uart_device(var, config)

    cg.add(var.set_frame_timeout_ms(config[CONF_FRAME_TIMEOUT_MS]))
    cg.add(var.set_max_frame_size(config[CONF_MAX_FRAME_SIZE]))
    cg.add(var.set_slave_address(config[CONF_SLAVE_ADDRESS]))
    cg.add(var.set_register_count(config[CONF_REGISTER_COUNT]))
    cg.add(var.set_stats_interval_ms(config[CONF_STATS_INTERVAL]))

    if CONF_REQUEST_COUNT in config:
        sens = await sensor.new_sensor(config[CONF_REQUEST_COUNT])
        cg.add(var.set_request_count_sensor(sens))
    if CONF_RESPONSE_COUNT in config:
        sens = await sensor.new_sensor(config[CONF_RESPONSE_COUNT])
        cg.add(var.set_response_count_sensor(sens))
    if CONF_CRC_ERROR_COUNT in config:
        sens = await sensor.new_sensor(config[CONF_CRC_ERROR_COUNT])
        cg.add(var.set_crc_error_sensor(sens))
