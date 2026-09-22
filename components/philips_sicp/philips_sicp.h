#pragma once

#include <deque>
#include <vector>

#include "esphome/components/number/number.h"
#include "esphome/components/select/select.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/switch/switch.h"
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

  // High-level actions used by child entities.
  void request_power(bool on);
  void request_input_by_index(size_t index);
  void request_volume(float value);
  void request_picture_format_by_index(size_t index);
  void request_pip(bool on);
  void request_pip_position_by_index(size_t index);
  void request_pip_source_by_index(size_t index);
  void request_video_param(size_t slot, float value);  // slot 0=brightness 1=contrast 2=sharpness

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

  void request_poll_once_();

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

}  // namespace esphome::philips_sicp
