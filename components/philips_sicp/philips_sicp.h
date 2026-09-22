#pragma once

#include <deque>
#include <vector>

#include "esphome/components/button/button.h"
#include "esphome/components/number/number.h"
#include "esphome/components/select/select.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/switch/switch.h"
#include "esphome/components/text_sensor/text_sensor.h"
#include "esphome/components/uart/uart.h"
#include "esphome/core/component.h"
#include "esphome/core/hal.h"
#include "esphome/core/log.h"

namespace esphome::philips_sicp {

// ---- SICP command codes (generic Philips SICP, see README) ----
static const uint8_t CMD_COMM_CONTROL = 0x00;  // report only: 06=ACK 15=NACK 18=NAV
static const uint8_t CMD_POWER_SET = 0x18;
static const uint8_t CMD_POWER_GET = 0x19;
static const uint8_t CMD_VIDEO_SET = 0x32;
static const uint8_t CMD_VIDEO_GET = 0x33;
static const uint8_t CMD_PICTURE_FORMAT_SET = 0x3A;
static const uint8_t CMD_PICTURE_FORMAT_GET = 0x3B;
static const uint8_t CMD_PIP_SET = 0x3C;
static const uint8_t CMD_VOLUME_SET = 0x44;
static const uint8_t CMD_VOLUME_GET = 0x45;
static const uint8_t CMD_PIP_SOURCE_SET = 0x84;
static const uint8_t CMD_PIP_SOURCE_GET = 0x85;
static const uint8_t CMD_MISC_GET = 0x0F;  // item 0x02 = operating hours
static const uint8_t CMD_TEMP_GET = 0x2F;
static const uint8_t CMD_INPUT_SET = 0xAC;
static const uint8_t CMD_INPUT_GET = 0xAD;
// Extended, documentation-derived or rarely-needed commands (see README for
// per-command support status; several are write-only or display-specific).
static const uint8_t CMD_VERSION_GET = 0xA2;      // which 0x00=SICP, 0x01=software
static const uint8_t CMD_INPUT_LOCK_SET = 0x1C;   // bit0=RC, bit1=keyboard (0=locked)
static const uint8_t CMD_INPUT_LOCK_GET = 0x1D;
static const uint8_t CMD_COLD_START_SET = 0xA3;   // 00=off 01=forced on 02=last (no GET)
static const uint8_t CMD_VOLUME_LIMITS_SET = 0xB8;  // min, max, switch-on (no GET)
static const uint8_t CMD_AUDIO_SET = 0x42;        // treble, bass
static const uint8_t CMD_AUDIO_GET = 0x43;
static const uint8_t CMD_SMARTPOWER_SET = 0xDD;   // level only (no GET)
static const uint8_t CMD_AUTO_ADJUST_SET = 0x70;  // VGA only: 40 00 (no GET)
static const uint8_t CMD_SERIAL_GET = 0x15;       // 14-char production code
static const uint8_t CMD_TILING_SET = 0x22;
static const uint8_t CMD_TILING_GET = 0x23;
static const uint8_t CMD_AUTOSIGNAL_GET = 0xAF;  // payload undocumented

static const uint8_t ACK_VALUE = 0x06;
static const uint8_t NACK_VALUE = 0x15;
static const uint8_t NAV_VALUE = 0x18;

// Extended TX framing (experimentally verified on hardware):
//   A6 01 00 00 00 | SIZE | 01 | CMD... | CHECKSUM
//   SIZE = command.size() + 2
//   CHECKSUM = 0xA7 ^ SIZE ^ 0x01 ^ each command byte
std::vector<uint8_t> build_extended_packet(const std::vector<uint8_t> &command);
uint8_t extended_checksum(const std::vector<uint8_t> &command);

class PhilipsSicp : public PollingComponent, public uart::UARTDevice {
 public:
  void setup() override;
  void loop() override;
  void update() override;
  void dump_config() override;

  void set_command_timeout(uint32_t ms) { this->command_timeout_ms_ = ms; }
  void set_max_retries(uint8_t n) { this->max_retries_ = n; }
  void set_command_gap(uint32_t ms) { this->command_gap_ms_ = ms; }

  // Entity registration (called from codegen).
  void set_power_switch(switch_::Switch *s) { this->power_switch_ = s; }
  void set_pip_switch(switch_::Switch *s) { this->pip_switch_ = s; }
  void set_input_select(select::Select *s) { this->input_select_ = s; }
  void set_picture_format_select(select::Select *s) { this->picture_format_select_ = s; }
  void set_pip_position_select(select::Select *s) { this->pip_position_select_ = s; }
  void set_pip_source_select(select::Select *s) { this->pip_source_select_ = s; }
  void set_volume_number(number::Number *n) { this->volume_number_ = n; }
  void set_brightness_number(number::Number *n) { this->brightness_number_ = n; }
  void set_contrast_number(number::Number *n) { this->contrast_number_ = n; }
  void set_sharpness_number(number::Number *n) { this->sharpness_number_ = n; }
  void set_operating_hours_sensor(sensor::Sensor *s) { this->operating_hours_sensor_ = s; }
  void set_temperature_sensor(sensor::Sensor *s) { this->temperature_sensor_ = s; }
  void set_sicp_version_text(text_sensor::TextSensor *s) { this->sicp_version_text_ = s; }
  void set_software_version_text(text_sensor::TextSensor *s) { this->software_version_text_ = s; }
  void set_remote_lock_switch(switch_::Switch *s) { this->remote_lock_switch_ = s; }
  void set_keyboard_lock_switch(switch_::Switch *s) { this->keyboard_lock_switch_ = s; }
  void set_cold_start_select(select::Select *s) { this->cold_start_select_ = s; }
  void set_treble_number(number::Number *n) { this->treble_number_ = n; }
  void set_bass_number(number::Number *n) { this->bass_number_ = n; }
  void set_min_volume_number(number::Number *n) { this->min_volume_number_ = n; }
  void set_max_volume_number(number::Number *n) { this->max_volume_number_ = n; }
  void set_switch_on_volume_number(number::Number *n) { this->switch_on_volume_number_ = n; }
  void set_smartpower_select(select::Select *s) { this->smartpower_select_ = s; }
  void set_serial_text(text_sensor::TextSensor *s) { this->serial_text_ = s; }
  void set_tiling_enable_switch(switch_::Switch *s) { this->tiling_enable_switch_ = s; }
  void set_tiling_frame_switch(switch_::Switch *s) { this->tiling_frame_switch_ = s; }
  void set_tiling_position_number(number::Number *n) { this->tiling_position_number_ = n; }
  void set_tiling_h_number(number::Number *n) { this->tiling_h_number_ = n; }
  void set_tiling_v_number(number::Number *n) { this->tiling_v_number_ = n; }

  // High-level actions used by child entities.
  void request_power(bool on);
  void request_input_by_index(size_t index);
  void request_volume(float value);
  void request_picture_format_by_index(size_t index);
  void request_pip(bool on);
  void request_pip_position_by_index(size_t index);
  void request_pip_source_by_index(size_t index);
  void request_video_param(size_t slot, float value);  // slot 0=brightness 1=contrast 2=sharpness
  // Extended commands (see README for support status).
  void request_input_lock(size_t slot, bool unlocked);  // slot 0=remote 1=keyboard
  void request_cold_start_by_index(size_t index);
  void request_volume_limit(size_t slot, float value);  // slot 0=min 1=max 2=switch-on
  void request_audio(size_t slot, float value);         // slot 0=treble 1=bass
  void request_smartpower_by_index(size_t index);
  void request_auto_adjust();
  void request_autosignal_probe();
  void request_tiling_enable(bool on);
  void request_tiling_frame(bool on);
  void request_tiling_geometry(size_t slot, float value);  // slot 0=pos 1=H 2=V

  // Tiling V/H monitor packing: code 0x01..0x19, (V-1)*5+(H-1)+1. Returns 0 if invalid.
  static uint8_t tiling_vh_to_code(uint8_t v, uint8_t h);
  static bool tiling_code_to_vh(uint8_t code, uint8_t *v, uint8_t *h);

  // Queue a raw SICP command (without framing). Thread-safe w.r.t. loop (same core).
  void enqueue_command(const std::vector<uint8_t> &command, bool expect_report = false);

  // Input source helpers (shared with selects).
  static const char *const *input_options();
  static size_t input_options_size();
  // Returns 5-byte AC payload for option index, or empty if invalid.
  static std::vector<uint8_t> input_set_payload(size_t index);
  // Map AD report (type, number) to option index. Returns -1 if unknown.
  static int input_report_to_index(uint8_t type, uint8_t number);

  static const char *const *picture_format_options();
  static size_t picture_format_options_size();
  static const char *const *pip_position_options();
  static size_t pip_position_options_size();
  static const char *const *cold_start_options();
  static size_t cold_start_options_size();
  static const char *const *smartpower_options();
  static size_t smartpower_options_size();

 protected:
  struct QueuedCommand {
    std::vector<uint8_t> sicp;
    bool expect_report{false};
    uint8_t retries_left{0};
  };
  struct Outstanding {
    bool active{false};
    std::vector<uint8_t> sicp;
    bool expect_report{false};
    uint8_t retries_left{0};
    uint32_t sent_at{0};
  };

  void pump_queue_();
  void transmit_(const std::vector<uint8_t> &sicp);
  void on_frame_(const std::vector<uint8_t> &sicp_payload, bool extended);
  void on_ack_report_(uint8_t value);
  void on_sicp_report_(const std::vector<uint8_t> &data);
  void handle_power_report_(const std::vector<uint8_t> &data);
  void handle_input_report_(const std::vector<uint8_t> &data);
  void handle_video_report_(const std::vector<uint8_t> &data);
  void handle_picture_format_report_(const std::vector<uint8_t> &data);
  void handle_volume_report_(const std::vector<uint8_t> &data);
  void handle_misc_report_(const std::vector<uint8_t> &data);
  void handle_pip_source_report_(const std::vector<uint8_t> &data);
  void handle_temp_report_(const std::vector<uint8_t> &data);
  void handle_version_report_(const std::vector<uint8_t> &data);
  void handle_input_lock_report_(const std::vector<uint8_t> &data);
  void handle_audio_report_(const std::vector<uint8_t> &data);
  void handle_serial_report_(const std::vector<uint8_t> &data);
  void handle_tiling_report_(const std::vector<uint8_t> &data);

  void request_poll_once_();
  void send_tiling_set_();
  std::deque<QueuedCommand> queue_;
  Outstanding outstanding_{};
  std::vector<uint8_t> rx_buf_{};
  uint32_t last_rx_activity_{0};
  uint32_t last_tx_at_{0};
  uint32_t command_timeout_ms_{500};
  uint32_t command_gap_ms_{60};
  uint8_t max_retries_{2};
  size_t poll_slot_{0};

  // Cached video params for read-modify-write (SET sets all fields at once).
  float brightness_{50};
  float contrast_{50};
  float sharpness_{50};
  bool video_cache_valid_{false};
  // Cached PIP state.
  bool pip_on_{false};
  uint8_t pip_position_{0};
  // Cached combined-SET state (write-only commands have no GET).
  bool remote_unlocked_{true};
  bool keyboard_unlocked_{true};
  float treble_{50};
  float bass_{50};
  bool audio_cache_valid_{false};
  float min_volume_{0};
  float max_volume_{100};
  float switch_on_volume_{50};
  // Tiling cache (GET exists, but SET needs all fields at once).
  bool tiling_enabled_{false};
  bool tiling_frame_{false};
  uint8_t tiling_position_{1};
  uint8_t tiling_h_{1};
  uint8_t tiling_v_{1};
  uint8_t last_version_label_{0};

  switch_::Switch *power_switch_{nullptr};
  switch_::Switch *pip_switch_{nullptr};
  select::Select *input_select_{nullptr};
  select::Select *picture_format_select_{nullptr};
  select::Select *pip_position_select_{nullptr};
  select::Select *pip_source_select_{nullptr};
  number::Number *volume_number_{nullptr};
  number::Number *brightness_number_{nullptr};
  number::Number *contrast_number_{nullptr};
  number::Number *sharpness_number_{nullptr};
  sensor::Sensor *operating_hours_sensor_{nullptr};
  sensor::Sensor *temperature_sensor_{nullptr};
  text_sensor::TextSensor *sicp_version_text_{nullptr};
  text_sensor::TextSensor *software_version_text_{nullptr};
  text_sensor::TextSensor *serial_text_{nullptr};
  switch_::Switch *remote_lock_switch_{nullptr};
  switch_::Switch *keyboard_lock_switch_{nullptr};
  switch_::Switch *tiling_enable_switch_{nullptr};
  switch_::Switch *tiling_frame_switch_{nullptr};
  select::Select *cold_start_select_{nullptr};
  select::Select *smartpower_select_{nullptr};
  number::Number *treble_number_{nullptr};
  number::Number *bass_number_{nullptr};
  number::Number *min_volume_number_{nullptr};
  number::Number *max_volume_number_{nullptr};
  number::Number *switch_on_volume_number_{nullptr};
  number::Number *tiling_position_number_{nullptr};
  number::Number *tiling_h_number_{nullptr};
  number::Number *tiling_v_number_{nullptr};
};

// ---- Child entities ----

class SicpPowerSwitch : public switch_::Switch {
 public:
  void set_parent(PhilipsSicp *p) { this->parent_ = p; }
 protected:
  void write_state(bool state) override;
  PhilipsSicp *parent_{nullptr};
};

class SicpPipSwitch : public switch_::Switch {
 public:
  void set_parent(PhilipsSicp *p) { this->parent_ = p; }
 protected:
  void write_state(bool state) override;
  PhilipsSicp *parent_{nullptr};
};

class SicpInputSelect : public select::Select {
 public:
  void set_parent(PhilipsSicp *p) { this->parent_ = p; }
 protected:
  void control(const std::string &value) override;
  PhilipsSicp *parent_{nullptr};
};

class SicpPictureFormatSelect : public select::Select {
 public:
  void set_parent(PhilipsSicp *p) { this->parent_ = p; }
 protected:
  void control(const std::string &value) override;
  PhilipsSicp *parent_{nullptr};
};

class SicpPipPositionSelect : public select::Select {
 public:
  void set_parent(PhilipsSicp *p) { this->parent_ = p; }
 protected:
  void control(const std::string &value) override;
  PhilipsSicp *parent_{nullptr};
};

class SicpPipSourceSelect : public select::Select {
 public:
  void set_parent(PhilipsSicp *p) { this->parent_ = p; }
 protected:
  void control(const std::string &value) override;
  PhilipsSicp *parent_{nullptr};
};

class SicpVolumeNumber : public number::Number {
 public:
  void set_parent(PhilipsSicp *p) { this->parent_ = p; }
 protected:
  void control(float value) override;
  PhilipsSicp *parent_{nullptr};
};

class SicpVideoParamNumber : public number::Number {
 public:
  void set_parent(PhilipsSicp *p) { this->parent_ = p; }
  void set_slot(uint8_t slot) { this->slot_ = slot; }
 protected:
  void control(float value) override;
  PhilipsSicp *parent_{nullptr};
  uint8_t slot_{0};
};

class SicpLockSwitch : public switch_::Switch {
 public:
  void set_parent(PhilipsSicp *p) { this->parent_ = p; }
  void set_slot(uint8_t slot) { this->slot_ = slot; }  // 0=remote 1=keyboard
 protected:
  void write_state(bool state) override;
  PhilipsSicp *parent_{nullptr};
  uint8_t slot_{0};
};

class SicpColdStartSelect : public select::Select {
 public:
  void set_parent(PhilipsSicp *p) { this->parent_ = p; }
 protected:
  void control(const std::string &value) override;
  PhilipsSicp *parent_{nullptr};
};

class SicpSmartPowerSelect : public select::Select {
 public:
  void set_parent(PhilipsSicp *p) { this->parent_ = p; }
 protected:
  void control(const std::string &value) override;
  PhilipsSicp *parent_{nullptr};
};

class SicpAudioNumber : public number::Number {
 public:
  void set_parent(PhilipsSicp *p) { this->parent_ = p; }
  void set_slot(uint8_t slot) { this->slot_ = slot; }  // 0=treble 1=bass
 protected:
  void control(float value) override;
  PhilipsSicp *parent_{nullptr};
  uint8_t slot_{0};
};

class SicpVolumeLimitNumber : public number::Number {
 public:
  void set_parent(PhilipsSicp *p) { this->parent_ = p; }
  void set_slot(uint8_t slot) { this->slot_ = slot; }  // 0=min 1=max 2=switch-on
 protected:
  void control(float value) override;
  PhilipsSicp *parent_{nullptr};
  uint8_t slot_{0};
};

class SicpAutoAdjustButton : public button::Button {
 public:
  void set_parent(PhilipsSicp *p) { this->parent_ = p; }
 protected:
  void press_action() override;
  PhilipsSicp *parent_{nullptr};
};

class SicpAutoSignalButton : public button::Button {
 public:
  void set_parent(PhilipsSicp *p) { this->parent_ = p; }
 protected:
  void press_action() override;
  PhilipsSicp *parent_{nullptr};
};

class SicpTilingEnableSwitch : public switch_::Switch {
 public:
  void set_parent(PhilipsSicp *p) { this->parent_ = p; }
 protected:
  void write_state(bool state) override;
  PhilipsSicp *parent_{nullptr};
};

class SicpTilingFrameSwitch : public switch_::Switch {
 public:
  void set_parent(PhilipsSicp *p) { this->parent_ = p; }
 protected:
  void write_state(bool state) override;
  PhilipsSicp *parent_{nullptr};
};

class SicpTilingNumber : public number::Number {
 public:
  void set_parent(PhilipsSicp *p) { this->parent_ = p; }
  void set_slot(uint8_t slot) { this->slot_ = slot; }  // 0=position 1=H 2=V
 protected:
  void control(float value) override;
  PhilipsSicp *parent_{nullptr};
  uint8_t slot_{0};
};

}  // namespace esphome::philips_sicp
