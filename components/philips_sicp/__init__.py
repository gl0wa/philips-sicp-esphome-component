import esphome.codegen as cg
from esphome.components import number, select, sensor, switch, uart
import esphome.config_validation as cv
from esphome.const import (
    CONF_ID,
    DEVICE_CLASS_DURATION,
    DEVICE_CLASS_TEMPERATURE,
    STATE_CLASS_MEASUREMENT,
    STATE_CLASS_TOTAL_INCREASING,
    UNIT_CELSIUS,
    UNIT_HOUR,
)

CODEOWNERS = ["@gl0wa"]
DEPENDENCIES = ["uart"]
AUTO_LOAD = ["switch", "select", "number", "sensor"]

DOMAIN = "philips_sicp"

philips_sicp_ns = cg.esphome_ns.namespace("philips_sicp")
PhilipsSicp = philips_sicp_ns.class_("PhilipsSicp", cg.PollingComponent, uart.UARTDevice)

SicpPowerSwitch = philips_sicp_ns.class_("SicpPowerSwitch", switch.Switch)
SicpPipSwitch = philips_sicp_ns.class_("SicpPipSwitch", switch.Switch)
SicpInputSelect = philips_sicp_ns.class_("SicpInputSelect", select.Select)
SicpPictureFormatSelect = philips_sicp_ns.class_("SicpPictureFormatSelect", select.Select)
SicpPipPositionSelect = philips_sicp_ns.class_("SicpPipPositionSelect", select.Select)
SicpPipSourceSelect = philips_sicp_ns.class_("SicpPipSourceSelect", select.Select)
SicpVolumeNumber = philips_sicp_ns.class_("SicpVolumeNumber", number.Number)
SicpVideoParamNumber = philips_sicp_ns.class_("SicpVideoParamNumber", number.Number)

CONF_POWER = "power"
CONF_INPUT = "input"
CONF_VOLUME = "volume"
CONF_PICTURE_FORMAT = "picture_format"
CONF_BRIGHTNESS = "brightness"
CONF_CONTRAST = "contrast"
CONF_SHARPNESS = "sharpness"
CONF_OPERATING_HOURS = "operating_hours"
CONF_TEMPERATURE = "temperature"
CONF_PIP = "pip"
CONF_PIP_POSITION = "pip_position"
CONF_PIP_SOURCE = "pip_source"
CONF_COMMAND_TIMEOUT = "command_timeout"
CONF_MAX_RETRIES = "max_retries"
CONF_COMMAND_GAP = "command_gap"

INPUT_OPTIONS = ["VGA", "DVI", "HDMI", "MHL-HDMI2", "DisplayPort", "Mini DisplayPort"]
PICTURE_FORMAT_OPTIONS = ["Normal", "Custom", "Real", "Full", "21:9", "Dynamic"]
PIP_POSITION_OPTIONS = ["Bottom Left", "Top Left", "Top Right", "Bottom Right"]


def _input_select_schema(class_):
    return select.select_schema(class_)


def _entity_schema_validator(options):
    # Select options are fixed by the component; reject user-supplied options.
    def validator(config):
        if "options" in config:
            raise cv.Invalid("options may not be set; they are fixed by the component")
        return config

    return validator


CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(PhilipsSicp),
            cv.Optional(CONF_POWER): switch.switch_schema(SicpPowerSwitch),
            cv.Optional(CONF_PIP): switch.switch_schema(SicpPipSwitch),
            cv.Optional(CONF_INPUT): _input_select_schema(SicpInputSelect),
            cv.Optional(CONF_PICTURE_FORMAT): _input_select_schema(SicpPictureFormatSelect),
            cv.Optional(CONF_PIP_POSITION): _input_select_schema(SicpPipPositionSelect),
            cv.Optional(CONF_PIP_SOURCE): _input_select_schema(SicpPipSourceSelect),
            cv.Optional(CONF_VOLUME): number.number_schema(
                SicpVolumeNumber, unit_of_measurement="%"
            ),
            cv.Optional(CONF_BRIGHTNESS): number.number_schema(
                SicpVideoParamNumber, unit_of_measurement="%"
            ),
            cv.Optional(CONF_CONTRAST): number.number_schema(
                SicpVideoParamNumber, unit_of_measurement="%"
            ),
            cv.Optional(CONF_SHARPNESS): number.number_schema(
                SicpVideoParamNumber, unit_of_measurement="%"
            ),
            cv.Optional(CONF_OPERATING_HOURS): sensor.sensor_schema(
                unit_of_measurement=UNIT_HOUR,
                accuracy_decimals=0,
                device_class=DEVICE_CLASS_DURATION,
                state_class=STATE_CLASS_TOTAL_INCREASING,
            ),
            cv.Optional(CONF_TEMPERATURE): sensor.sensor_schema(
                unit_of_measurement=UNIT_CELSIUS,
                accuracy_decimals=0,
                device_class=DEVICE_CLASS_TEMPERATURE,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            cv.Optional(CONF_COMMAND_TIMEOUT, default="500ms"): cv.positive_time_period_milliseconds,
            cv.Optional(CONF_MAX_RETRIES, default=2): cv.int_range(min=0, max=5),
            cv.Optional(CONF_COMMAND_GAP, default="60ms"): cv.positive_time_period_milliseconds,
        }
    )
    .extend(cv.polling_component_schema("30s"))
    .extend(uart.UART_DEVICE_SCHEMA)
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await uart.register_uart_device(var, config)

    cg.add(var.set_command_timeout(config[CONF_COMMAND_TIMEOUT]))
    cg.add(var.set_max_retries(config[CONF_MAX_RETRIES]))
    cg.add(var.set_command_gap(config[CONF_COMMAND_GAP]))

    if CONF_POWER in config:
        sw = await switch.new_switch(config[CONF_POWER])
        cg.add(sw.set_parent(var))
        cg.add(var.set_power_switch(sw))

    if CONF_PIP in config:
        sw = await switch.new_switch(config[CONF_PIP])
        cg.add(sw.set_parent(var))
        cg.add(var.set_pip_switch(sw))

    if CONF_INPUT in config:
        sel = await select.new_select(config[CONF_INPUT], options=INPUT_OPTIONS)
        cg.add(sel.set_parent(var))
        cg.add(var.set_input_select(sel))

    if CONF_PICTURE_FORMAT in config:
        sel = await select.new_select(config[CONF_PICTURE_FORMAT], options=PICTURE_FORMAT_OPTIONS)
        cg.add(sel.set_parent(var))
        cg.add(var.set_picture_format_select(sel))

    if CONF_PIP_POSITION in config:
        sel = await select.new_select(config[CONF_PIP_POSITION], options=PIP_POSITION_OPTIONS)
        cg.add(sel.set_parent(var))
        cg.add(var.set_pip_position_select(sel))

    if CONF_PIP_SOURCE in config:
        sel = await select.new_select(config[CONF_PIP_SOURCE], options=INPUT_OPTIONS)
        cg.add(sel.set_parent(var))
        cg.add(var.set_pip_source_select(sel))

    if CONF_VOLUME in config:
        num = await number.new_number(config[CONF_VOLUME], min_value=0, max_value=100, step=1)
        cg.add(num.set_parent(var))
        cg.add(var.set_volume_number(num))

    if CONF_BRIGHTNESS in config:
        num = await number.new_number(config[CONF_BRIGHTNESS], min_value=0, max_value=100, step=1)
        cg.add(num.set_parent(var))
        cg.add(num.set_slot(0))
        cg.add(var.set_brightness_number(num))

    if CONF_CONTRAST in config:
        num = await number.new_number(config[CONF_CONTRAST], min_value=0, max_value=100, step=1)
        cg.add(num.set_parent(var))
        cg.add(num.set_slot(1))
        cg.add(var.set_contrast_number(num))

    if CONF_SHARPNESS in config:
        num = await number.new_number(config[CONF_SHARPNESS], min_value=0, max_value=100, step=1)
        cg.add(num.set_parent(var))
        cg.add(num.set_slot(2))
        cg.add(var.set_sharpness_number(num))

    if CONF_OPERATING_HOURS in config:
        sens = await sensor.new_sensor(config[CONF_OPERATING_HOURS])
        cg.add(var.set_operating_hours_sensor(sens))

    if CONF_TEMPERATURE in config:
        sens = await sensor.new_sensor(config[CONF_TEMPERATURE])
        cg.add(var.set_temperature_sensor(sens))
