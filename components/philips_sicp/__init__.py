import esphome.codegen as cg
from esphome.components import button, number, select, sensor, switch, text_sensor, uart
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
AUTO_LOAD = ["switch", "select", "number", "sensor", "button", "text_sensor"]

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
SicpAudioNumber = philips_sicp_ns.class_("SicpAudioNumber", number.Number)
SicpVolumeLimitNumber = philips_sicp_ns.class_("SicpVolumeLimitNumber", number.Number)
SicpTilingNumber = philips_sicp_ns.class_("SicpTilingNumber", number.Number)
SicpLockSwitch = philips_sicp_ns.class_("SicpLockSwitch", switch.Switch)
SicpTilingEnableSwitch = philips_sicp_ns.class_("SicpTilingEnableSwitch", switch.Switch)
SicpTilingFrameSwitch = philips_sicp_ns.class_("SicpTilingFrameSwitch", switch.Switch)
SicpColdStartSelect = philips_sicp_ns.class_("SicpColdStartSelect", select.Select)
SicpSmartPowerSelect = philips_sicp_ns.class_("SicpSmartPowerSelect", select.Select)
SicpAutoAdjustButton = philips_sicp_ns.class_("SicpAutoAdjustButton", button.Button)
SicpAutoSignalButton = philips_sicp_ns.class_("SicpAutoSignalButton", button.Button)

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
CONF_SICP_VERSION = "sicp_version"
CONF_SOFTWARE_VERSION = "software_version"
CONF_SERIAL = "serial"
CONF_REMOTE_LOCK = "remote_lock"
CONF_KEYBOARD_LOCK = "keyboard_lock"
CONF_COLD_START = "cold_start"
CONF_TREBLE = "treble"
CONF_BASS = "bass"
CONF_MIN_VOLUME = "min_volume"
CONF_MAX_VOLUME = "max_volume"
CONF_SWITCH_ON_VOLUME = "switch_on_volume"
CONF_SMARTPOWER = "smartpower"
CONF_AUTO_ADJUST = "auto_adjust"
CONF_AUTOSIGNAL_PROBE = "autosignal_probe"
CONF_TILING = "tiling"
CONF_TILING_FRAME = "tiling_frame_comp"
CONF_TILING_POSITION = "tiling_position"
CONF_TILING_H = "tiling_h_monitors"
CONF_TILING_V = "tiling_v_monitors"

INPUT_OPTIONS = ["VGA", "DVI", "HDMI", "MHL-HDMI2", "DisplayPort", "Mini DisplayPort"]
PICTURE_FORMAT_OPTIONS = ["Normal", "Custom", "Real", "Full", "21:9", "Dynamic"]
PIP_POSITION_OPTIONS = ["Bottom Left", "Top Left", "Top Right", "Bottom Right"]
COLD_START_OPTIONS = ["Off", "Forced On", "Last Status"]
SMARTPOWER_OPTIONS = ["Off", "Low", "Medium", "High"]


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
            cv.Optional(CONF_SICP_VERSION): text_sensor.text_sensor_schema(),
            cv.Optional(CONF_SOFTWARE_VERSION): text_sensor.text_sensor_schema(),
            cv.Optional(CONF_SERIAL): text_sensor.text_sensor_schema(),
            cv.Optional(CONF_REMOTE_LOCK): switch.switch_schema(SicpLockSwitch),
            cv.Optional(CONF_KEYBOARD_LOCK): switch.switch_schema(SicpLockSwitch),
            cv.Optional(CONF_COLD_START): _input_select_schema(SicpColdStartSelect),
            cv.Optional(CONF_SMARTPOWER): _input_select_schema(SicpSmartPowerSelect),
            cv.Optional(CONF_TREBLE): number.number_schema(
                SicpAudioNumber, unit_of_measurement="%"
            ),
            cv.Optional(CONF_BASS): number.number_schema(
                SicpAudioNumber, unit_of_measurement="%"
            ),
            cv.Optional(CONF_MIN_VOLUME): number.number_schema(
                SicpVolumeLimitNumber, unit_of_measurement="%"
            ),
            cv.Optional(CONF_MAX_VOLUME): number.number_schema(
                SicpVolumeLimitNumber, unit_of_measurement="%"
            ),
            cv.Optional(CONF_SWITCH_ON_VOLUME): number.number_schema(
                SicpVolumeLimitNumber, unit_of_measurement="%"
            ),
            cv.Optional(CONF_AUTO_ADJUST): button.button_schema(SicpAutoAdjustButton),
            cv.Optional(CONF_AUTOSIGNAL_PROBE): button.button_schema(SicpAutoSignalButton),
            cv.Optional(CONF_TILING): switch.switch_schema(SicpTilingEnableSwitch),
            cv.Optional(CONF_TILING_FRAME): switch.switch_schema(SicpTilingFrameSwitch),
            cv.Optional(CONF_TILING_POSITION): number.number_schema(SicpTilingNumber),
            cv.Optional(CONF_TILING_H): number.number_schema(SicpTilingNumber),
            cv.Optional(CONF_TILING_V): number.number_schema(SicpTilingNumber),
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

    if CONF_SICP_VERSION in config:
        ts = await text_sensor.new_text_sensor(config[CONF_SICP_VERSION])
        cg.add(var.set_sicp_version_text(ts))

    if CONF_SOFTWARE_VERSION in config:
        ts = await text_sensor.new_text_sensor(config[CONF_SOFTWARE_VERSION])
        cg.add(var.set_software_version_text(ts))

    if CONF_SERIAL in config:
        ts = await text_sensor.new_text_sensor(config[CONF_SERIAL])
        cg.add(var.set_serial_text(ts))

    if CONF_REMOTE_LOCK in config:
        sw = await switch.new_switch(config[CONF_REMOTE_LOCK])
        cg.add(sw.set_parent(var))
        cg.add(sw.set_slot(0))
        cg.add(var.set_remote_lock_switch(sw))

    if CONF_KEYBOARD_LOCK in config:
        sw = await switch.new_switch(config[CONF_KEYBOARD_LOCK])
        cg.add(sw.set_parent(var))
        cg.add(sw.set_slot(1))
        cg.add(var.set_keyboard_lock_switch(sw))

    if CONF_COLD_START in config:
        sel = await select.new_select(config[CONF_COLD_START], options=COLD_START_OPTIONS)
        cg.add(sel.set_parent(var))
        cg.add(var.set_cold_start_select(sel))

    if CONF_SMARTPOWER in config:
        sel = await select.new_select(config[CONF_SMARTPOWER], options=SMARTPOWER_OPTIONS)
        cg.add(sel.set_parent(var))
        cg.add(var.set_smartpower_select(sel))

    if CONF_TREBLE in config:
        num = await number.new_number(config[CONF_TREBLE], min_value=0, max_value=100, step=1)
        cg.add(num.set_parent(var))
        cg.add(num.set_slot(0))
        cg.add(var.set_treble_number(num))

    if CONF_BASS in config:
        num = await number.new_number(config[CONF_BASS], min_value=0, max_value=100, step=1)
        cg.add(num.set_parent(var))
        cg.add(num.set_slot(1))
        cg.add(var.set_bass_number(num))

    if CONF_MIN_VOLUME in config:
        num = await number.new_number(config[CONF_MIN_VOLUME], min_value=0, max_value=100, step=1)
        cg.add(num.set_parent(var))
        cg.add(num.set_slot(0))
        cg.add(var.set_min_volume_number(num))

    if CONF_MAX_VOLUME in config:
        num = await number.new_number(config[CONF_MAX_VOLUME], min_value=0, max_value=100, step=1)
        cg.add(num.set_parent(var))
        cg.add(num.set_slot(1))
        cg.add(var.set_max_volume_number(num))

    if CONF_SWITCH_ON_VOLUME in config:
        num = await number.new_number(
            config[CONF_SWITCH_ON_VOLUME], min_value=0, max_value=100, step=1
        )
        cg.add(num.set_parent(var))
        cg.add(num.set_slot(2))
        cg.add(var.set_switch_on_volume_number(num))

    if CONF_AUTO_ADJUST in config:
        btn = await button.new_button(config[CONF_AUTO_ADJUST])
        cg.add(btn.set_parent(var))

    if CONF_AUTOSIGNAL_PROBE in config:
        btn = await button.new_button(config[CONF_AUTOSIGNAL_PROBE])
        cg.add(btn.set_parent(var))

    if CONF_TILING in config:
        sw = await switch.new_switch(config[CONF_TILING])
        cg.add(sw.set_parent(var))
        cg.add(var.set_tiling_enable_switch(sw))

    if CONF_TILING_FRAME in config:
        sw = await switch.new_switch(config[CONF_TILING_FRAME])
        cg.add(sw.set_parent(var))
        cg.add(var.set_tiling_frame_switch(sw))

    if CONF_TILING_POSITION in config:
        num = await number.new_number(
            config[CONF_TILING_POSITION], min_value=1, max_value=25, step=1
        )
        cg.add(num.set_parent(var))
        cg.add(num.set_slot(0))
        cg.add(var.set_tiling_position_number(num))

    if CONF_TILING_H in config:
        num = await number.new_number(config[CONF_TILING_H], min_value=1, max_value=5, step=1)
        cg.add(num.set_parent(var))
        cg.add(num.set_slot(1))
        cg.add(var.set_tiling_h_number(num))

    if CONF_TILING_V in config:
        num = await number.new_number(config[CONF_TILING_V], min_value=1, max_value=5, step=1)
        cg.add(num.set_parent(var))
        cg.add(num.set_slot(2))
        cg.add(var.set_tiling_v_number(num))
