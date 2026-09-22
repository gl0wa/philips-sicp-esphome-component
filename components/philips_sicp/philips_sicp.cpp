#include "philips_sicp.h"

#include "esphome/core/log.h"

namespace esphome::philips_sicp {

static const char *const TAG = "philips_sicp";

static const char *const INPUT_OPTIONS[] = {"VGA", "DVI", "HDMI", "MHL-HDMI2", "DisplayPort",
                                            "Mini DisplayPort"};
static const size_t INPUT_OPTIONS_COUNT = 6;

// SET payloads: {0xAC, type, number, osd, mute}
static const uint8_t INPUT_SET_TABLE[6][5] = {
    {0xAC, 0x05, 0x00, 0x01, 0x00},  // VGA
    {0xAC, 0x07, 0x01, 0x01, 0x00},  // DVI (documentation-derived, untested)
    {0xAC, 0x06, 0x02, 0x01, 0x00},  // HDMI
    {0xAC, 0x06, 0x03, 0x01, 0x00},  // MHL-HDMI2
    {0xAC, 0x09, 0x04, 0x01, 0x00},  // DisplayPort
    {0xAC, 0x09, 0x05, 0x01, 0x00},  // Mini DisplayPort
};

static const char *const PICTURE_FORMAT_OPTIONS[] = {"Normal", "Custom", "Real", "Full", "21:9",
                                                     "Dynamic"};
static const size_t PICTURE_FORMAT_OPTIONS_COUNT = 6;

static const char *const PIP_POSITION_OPTIONS[] = {"Bottom Left", "Top Left", "Top Right",
                                                   "Bottom Right"};
static const size_t PIP_POSITION_OPTIONS_COUNT = 4;

static const char *const COLD_START_OPTIONS[] = {"Off", "Forced On", "Last Status"};
static const size_t COLD_START_OPTIONS_COUNT = 3;

static const char *const SMARTPOWER_OPTIONS[] = {"Off", "Low", "Medium", "High"};
static const size_t SMARTPOWER_OPTIONS_COUNT = 4;

static std::string hex_dump(const std::vector<uint8_t> &v) {
  char buf[8];
  std::string s;
  s.reserve(v.size() * 3);
  for (size_t i = 0; i < v.size(); i++) {
    snprintf(buf, sizeof(buf), "%02X", v[i]);
    s += buf;
    if (i + 1 < v.size())
      s += ' ';
  }
  return s;
}

uint8_t extended_checksum(const std::vector<uint8_t> &command) {
  uint8_t checksum = 0xA7;
  uint8_t size = static_cast<uint8_t>(command.size() + 2);
  checksum ^= size;
  checksum ^= 0x01;
  for (uint8_t b : command)
    checksum ^= b;
  return checksum;
}

std::vector<uint8_t> build_extended_packet(const std::vector<uint8_t> &command) {
  std::vector<uint8_t> out;
  out.reserve(8 + command.size());
  out.push_back(0xA6);
  out.push_back(0x01);
  out.push_back(0x00);
  out.push_back(0x00);
  out.push_back(0x00);
  uint8_t size = static_cast<uint8_t>(command.size() + 2);
  out.push_back(size);
  out.push_back(0x01);
  for (uint8_t b : command)
    out.push_back(b);
  out.push_back(extended_checksum(command));
  return out;
}

const char *const *PhilipsSicp::input_options() { return INPUT_OPTIONS; }
size_t PhilipsSicp::input_options_size() { return INPUT_OPTIONS_COUNT; }

std::vector<uint8_t> PhilipsSicp::input_set_payload(size_t index) {
  std::vector<uint8_t> out;
  if (index >= INPUT_OPTIONS_COUNT)
    return out;
  for (int i = 0; i < 5; i++)
    out.push_back(INPUT_SET_TABLE[index][i]);
  return out;
}

int PhilipsSicp::input_report_to_index(uint8_t type, uint8_t number) {
  if (type != 0xFD)
    return -1;
  if (number <= 0x05)
    return static_cast<int>(number);
  return -1;
}

const char *const *PhilipsSicp::picture_format_options() { return PICTURE_FORMAT_OPTIONS; }
size_t PhilipsSicp::picture_format_options_size() { return PICTURE_FORMAT_OPTIONS_COUNT; }
const char *const *PhilipsSicp::pip_position_options() { return PIP_POSITION_OPTIONS; }
size_t PhilipsSicp::pip_position_options_size() { return PIP_POSITION_OPTIONS_COUNT; }
const char *const *PhilipsSicp::cold_start_options() { return COLD_START_OPTIONS; }
size_t PhilipsSicp::cold_start_options_size() { return COLD_START_OPTIONS_COUNT; }
const char *const *PhilipsSicp::smartpower_options() { return SMARTPOWER_OPTIONS; }
size_t PhilipsSicp::smartpower_options_size() { return SMARTPOWER_OPTIONS_COUNT; }

uint8_t PhilipsSicp::tiling_vh_to_code(uint8_t v, uint8_t h) {
  if (v < 1 || v > 5 || h < 1 || h > 5)
    return 0;
  return (uint8_t) ((v - 1) * 5 + (h - 1) + 1);
}

bool PhilipsSicp::tiling_code_to_vh(uint8_t code, uint8_t *v, uint8_t *h) {
  if (code < 0x01 || code > 0x19 || v == nullptr || h == nullptr)
    return false;
  uint8_t idx = (uint8_t) (code - 1);
  *v = (uint8_t) (idx / 5 + 1);
  *h = (uint8_t) (idx % 5 + 1);
  return true;
}

void PhilipsSicp::setup() {
  this->rx_buf_.reserve(64);
  this->last_tx_at_ = millis() - this->command_gap_ms_ - 1;
  ESP_LOGCONFIG(TAG, "Philips SICP hub setup (timeout=%ums retries=%u gap=%ums)",
                (unsigned) this->command_timeout_ms_, this->max_retries_,
                (unsigned) this->command_gap_ms_);
}

void PhilipsSicp::dump_config() {
  ESP_LOGCONFIG(TAG, "Philips SICP:");
  LOG_UPDATE_INTERVAL(this);
  if (this->power_switch_ != nullptr)
    LOG_SWITCH("  ", "Power", this->power_switch_);
  if (this->pip_switch_ != nullptr)
    LOG_SWITCH("  ", "PIP", this->pip_switch_);
  if (this->input_select_ != nullptr)
    LOG_SELECT("  ", "Input", this->input_select_);
  if (this->picture_format_select_ != nullptr)
    LOG_SELECT("  ", "Picture format", this->picture_format_select_);
  if (this->pip_position_select_ != nullptr)
    LOG_SELECT("  ", "PIP position", this->pip_position_select_);
  if (this->pip_source_select_ != nullptr)
    LOG_SELECT("  ", "PIP source", this->pip_source_select_);
  if (this->volume_number_ != nullptr)
    LOG_NUMBER("  ", "Volume", this->volume_number_);
  if (this->brightness_number_ != nullptr)
    LOG_NUMBER("  ", "Brightness", this->brightness_number_);
  if (this->contrast_number_ != nullptr)
    LOG_NUMBER("  ", "Contrast", this->contrast_number_);
  if (this->sharpness_number_ != nullptr)
    LOG_NUMBER("  ", "Sharpness", this->sharpness_number_);
  if (this->operating_hours_sensor_ != nullptr)
    LOG_SENSOR("  ", "Operating hours", this->operating_hours_sensor_);
  if (this->temperature_sensor_ != nullptr)
    LOG_SENSOR("  ", "Temperature", this->temperature_sensor_);
  if (this->sicp_version_text_ != nullptr)
    LOG_TEXT_SENSOR("  ", "SICP version", this->sicp_version_text_);
  if (this->software_version_text_ != nullptr)
    LOG_TEXT_SENSOR("  ", "Software version", this->software_version_text_);
  if (this->serial_text_ != nullptr)
    LOG_TEXT_SENSOR("  ", "Serial code", this->serial_text_);
  if (this->remote_lock_switch_ != nullptr)
    LOG_SWITCH("  ", "Remote lock", this->remote_lock_switch_);
  if (this->keyboard_lock_switch_ != nullptr)
    LOG_SWITCH("  ", "Keyboard lock", this->keyboard_lock_switch_);
  if (this->cold_start_select_ != nullptr)
    LOG_SELECT("  ", "Cold start", this->cold_start_select_);
  if (this->smartpower_select_ != nullptr)
    LOG_SELECT("  ", "SmartPower", this->smartpower_select_);
  if (this->treble_number_ != nullptr)
    LOG_NUMBER("  ", "Treble", this->treble_number_);
  if (this->bass_number_ != nullptr)
    LOG_NUMBER("  ", "Bass", this->bass_number_);
  if (this->min_volume_number_ != nullptr)
    LOG_NUMBER("  ", "Min volume", this->min_volume_number_);
  if (this->max_volume_number_ != nullptr)
    LOG_NUMBER("  ", "Max volume", this->max_volume_number_);
  if (this->switch_on_volume_number_ != nullptr)
    LOG_NUMBER("  ", "Switch-on volume", this->switch_on_volume_number_);
  if (this->tiling_enable_switch_ != nullptr)
    LOG_SWITCH("  ", "Tiling", this->tiling_enable_switch_);
  if (this->tiling_frame_switch_ != nullptr)
    LOG_SWITCH("  ", "Tiling frame comp", this->tiling_frame_switch_);
  if (this->tiling_position_number_ != nullptr)
    LOG_NUMBER("  ", "Tiling position", this->tiling_position_number_);
  if (this->tiling_h_number_ != nullptr)
    LOG_NUMBER("  ", "Tiling H monitors", this->tiling_h_number_);
  if (this->tiling_v_number_ != nullptr)
    LOG_NUMBER("  ", "Tiling V monitors", this->tiling_v_number_);
}

void PhilipsSicp::enqueue_command(const std::vector<uint8_t> &command, bool expect_report) {
  if (command.empty())
    return;
  if (this->queue_.size() >= 20) {
    ESP_LOGW(TAG, "Command queue full, dropping oldest");
    this->queue_.pop_front();
  }
  QueuedCommand q;
  q.sicp = command;
  q.expect_report = expect_report;
  q.retries_left = this->max_retries_;
  this->queue_.push_back(q);
  ESP_LOGV(TAG, "Enqueued SICP [%s] expect_report=%d", hex_dump(command).c_str(), (int) expect_report);
}

void PhilipsSicp::transmit_(const std::vector<uint8_t> &sicp) {
  auto packet = build_extended_packet(sicp);
  this->write_array(packet);
  this->last_tx_at_ = millis();
  ESP_LOGD(TAG, "TX SICP [%s] -> wire [%s]", hex_dump(sicp).c_str(), hex_dump(packet).c_str());
}

void PhilipsSicp::pump_queue_() {
  uint32_t now = millis();
  if (this->outstanding_.active) {
    if (now - this->outstanding_.sent_at >= this->command_timeout_ms_) {
      if (this->outstanding_.retries_left > 0) {
        this->outstanding_.retries_left--;
        this->outstanding_.sent_at = now;
        ESP_LOGD(TAG, "Timeout waiting for reply to [%s], retrying (%u left)",
                 hex_dump(this->outstanding_.sicp).c_str(), this->outstanding_.retries_left);
        this->transmit_(this->outstanding_.sicp);
        // transmit_ updates last_tx_at_; fix sent_at to match
        this->outstanding_.sent_at = this->last_tx_at_;
      } else {
        ESP_LOGW(TAG, "Timeout waiting for reply to [%s], giving up",
                 hex_dump(this->outstanding_.sicp).c_str());
        this->outstanding_.active = false;
      }
    }
    return;
  }
  if (this->queue_.empty())
    return;
  if (now - this->last_tx_at_ < this->command_gap_ms_)
    return;
  QueuedCommand q = this->queue_.front();
  this->queue_.pop_front();
  this->outstanding_.active = true;
  this->outstanding_.sicp = q.sicp;
  this->outstanding_.expect_report = q.expect_report;
  this->outstanding_.retries_left = q.retries_left;
  this->outstanding_.sent_at = now;
  this->transmit_(q.sicp);
  this->outstanding_.sent_at = this->last_tx_at_;
}

void PhilipsSicp::loop() {
  // Drain UART.
  bool got_bytes = false;
  while (this->available()) {
    uint8_t b;
    if (!this->read_byte(&b))
      break;
    this->rx_buf_.push_back(b);
    got_bytes = true;
  }
  if (got_bytes) {
    this->last_rx_activity_ = millis();
    ESP_LOGV(TAG, "RX raw (%u bytes total): [%s]", (unsigned) this->rx_buf_.size(),
             hex_dump(this->rx_buf_).c_str());
  }

  // Try to parse complete frames.
  bool progress = true;
  while (progress && !this->rx_buf_.empty()) {
    progress = false;
    // Extended framing (observed on hardware, both directions):
    //   TX host->display: A6 01 00 00 00 SIZE 01 CMD... CHECKSUM
    //   RX display->host: 21 01 00 00 SIZE 01 CMD... CHECKSUM
    //   SIZE = len(CMD) + 2, CHECKSUM = XOR of every preceding frame byte.
    //   (For TX the 0xA7-based formula from the legacy implementation is
    //   identical, since A6^01^00^00^00 == A7.)
    uint8_t magic = this->rx_buf_[0];
    size_t hdr = 0;  // index of the SIZE byte
    if (this->rx_buf_.size() >= 7 && (magic == 0xA6 || magic == 0x21) && this->rx_buf_[1] == 0x01 &&
        this->rx_buf_[2] == 0x00 && this->rx_buf_[3] == 0x00 &&
        (magic == 0x21 || this->rx_buf_[4] == 0x00)) {
      hdr = (magic == 0xA6) ? 5 : 4;
      uint8_t size = this->rx_buf_[hdr];
      size_t total = (size_t) size + hdr + 1;
      if (total < 8 || total > 64) {
        ESP_LOGW(TAG, "RX extended frame has implausible SIZE %02X, dropping lead byte", size);
        this->rx_buf_.erase(this->rx_buf_.begin());
        progress = true;
        continue;
      }
      if (this->rx_buf_.size() < total)
        break;  // wait for more bytes
      std::vector<uint8_t> frame(this->rx_buf_.begin(), this->rx_buf_.begin() + total);
      uint8_t ctrl = frame[hdr + 1];
      size_t cmdlen = (size_t) size - 2;
      std::vector<uint8_t> cmd(frame.begin() + hdr + 2, frame.begin() + hdr + 2 + cmdlen);
      uint8_t rx_sum = frame[total - 1];
      uint8_t calc = 0;
      for (size_t i = 0; i + 1 < total; i++)
        calc ^= frame[i];
      // Note: header control bytes are fixed; warn if inner control != 0x01.
      if (ctrl != 0x01)
        ESP_LOGD(TAG, "RX extended inner control is %02X (expected 01)", ctrl);
      if (rx_sum == calc) {
        ESP_LOGD(TAG, "RX extended [%s] cmd [%s]", hex_dump(frame).c_str(), hex_dump(cmd).c_str());
        this->rx_buf_.erase(this->rx_buf_.begin(), this->rx_buf_.begin() + total);
        this->on_frame_(cmd, true);
        progress = true;
        continue;
      }
      ESP_LOGW(TAG, "RX extended checksum mismatch: got %02X want %02X in [%s]", rx_sum, calc,
               hex_dump(frame).c_str());
      this->rx_buf_.erase(this->rx_buf_.begin());
      progress = true;
      continue;
    }
    // Generic SICP framing: MsgSize Control Data... Checksum, MsgSize = total length.
    uint8_t first = this->rx_buf_[0];
    if (first >= 0x03 && first <= 0x28) {
      size_t total = first;
      if (this->rx_buf_.size() < total)
        break;  // wait for more
      std::vector<uint8_t> frame(this->rx_buf_.begin(), this->rx_buf_.begin() + total);
      uint8_t calc = 0;
      for (size_t i = 0; i + 1 < total; i++)
        calc ^= frame[i];
      if (calc == frame[total - 1]) {
        std::vector<uint8_t> data;
        if (total >= 3)
          data.assign(frame.begin() + 2, frame.begin() + total - 1);
        ESP_LOGD(TAG, "RX generic [%s] data [%s]", hex_dump(frame).c_str(), hex_dump(data).c_str());
        this->rx_buf_.erase(this->rx_buf_.begin(), this->rx_buf_.begin() + total);
        this->on_frame_(data, false);
        progress = true;
        continue;
      }
      ESP_LOGW(TAG, "RX generic checksum mismatch in [%s]", hex_dump(frame).c_str());
      this->rx_buf_.erase(this->rx_buf_.begin());
      progress = true;
      continue;
    }
    // Unknown lead byte: could be noise or an undocumented framing. Log once at DEBUG
    // and resync. Raw bytes remain visible at VERY_VERBOSE above.
    ESP_LOGV(TAG, "RX resync: dropping lead byte %02X", first);
    this->rx_buf_.erase(this->rx_buf_.begin());
    progress = true;
  }

  // Stale partial frame guard.
  if (!this->rx_buf_.empty() && millis() - this->last_rx_activity_ > 500) {
    ESP_LOGD(TAG, "RX stale %u bytes, discarding: [%s]", (unsigned) this->rx_buf_.size(),
             hex_dump(this->rx_buf_).c_str());
    this->rx_buf_.clear();
  }

  this->pump_queue_();
}

void PhilipsSicp::on_frame_(const std::vector<uint8_t> &payload, bool /*extended*/) {
  if (payload.empty())
    return;
  if (payload[0] == CMD_COMM_CONTROL && payload.size() >= 2) {
    this->on_ack_report_(payload[1]);
    return;
  }
  // If we were waiting for this report, complete the outstanding command.
  if (this->outstanding_.active && this->outstanding_.expect_report && !this->outstanding_.sicp.empty() &&
      payload[0] == this->outstanding_.sicp[0]) {
    ESP_LOGD(TAG, "Report [%s] matches outstanding GET", hex_dump(payload).c_str());
    this->outstanding_.active = false;
  }
  this->on_sicp_report_(payload);
}

void PhilipsSicp::on_ack_report_(uint8_t value) {
  if (value == ACK_VALUE || value == 0x00) {
    // NOTE: the SICP document specifies ACK as 00 06, but observed hardware
    // replies to successful SET commands with 00 00. Accept both.
    ESP_LOGD(TAG, "SET acknowledged (00 %02X)", value);
    this->outstanding_.active = false;
    return;
  }
  if (value == NACK_VALUE) {
    ESP_LOGW(TAG, "NACK received for [%s]",
             this->outstanding_.active ? hex_dump(this->outstanding_.sicp).c_str() : "<idle>");
    // Transmission error: retry the outstanding SET if possible.
    if (this->outstanding_.active && !this->outstanding_.expect_report) {
      if (this->outstanding_.retries_left > 0) {
        this->outstanding_.retries_left--;
        this->outstanding_.sent_at = millis();
        this->transmit_(this->outstanding_.sicp);
        this->outstanding_.sent_at = this->last_tx_at_;
      } else {
        this->outstanding_.active = false;
      }
    }
    return;
  }
  if (value == NAV_VALUE) {
    ESP_LOGW(TAG, "NAV (not available) received for [%s]",
             this->outstanding_.active ? hex_dump(this->outstanding_.sicp).c_str() : "<idle>");
    this->outstanding_.active = false;
    return;
  }
  // Observed on hardware: e.g. a temperature GET on a display without that
  // sensor is answered with 00 03. Treat unknown comm-control values as
  // terminal for this attempt instead of retrying.
  ESP_LOGW(TAG, "Unknown comm-control value %02X", value);
  // Terminal for this attempt: the display answered with something we do not
  // understand (e.g. an undocumented error reply), retrying will not help.
  this->outstanding_.active = false;
}

void PhilipsSicp::on_sicp_report_(const std::vector<uint8_t> &data) {
  if (data.empty())
    return;
  ESP_LOGD(TAG, "Report [%s]", hex_dump(data).c_str());
  switch (data[0]) {
    case CMD_POWER_GET:
      this->handle_power_report_(data);
      break;
    case CMD_INPUT_GET:
      this->handle_input_report_(data);
      break;
    case CMD_VIDEO_GET:
      this->handle_video_report_(data);
      break;
    case CMD_PICTURE_FORMAT_GET:
      this->handle_picture_format_report_(data);
      break;
    case CMD_VOLUME_GET:
      this->handle_volume_report_(data);
      break;
    case CMD_MISC_GET:
      this->handle_misc_report_(data);
      break;
    case CMD_PIP_SOURCE_GET:
      this->handle_pip_source_report_(data);
      break;
    case CMD_TEMP_GET:
      this->handle_temp_report_(data);
      break;
    case CMD_VERSION_GET:
      this->handle_version_report_(data);
      break;
    case CMD_INPUT_LOCK_GET:
      this->handle_input_lock_report_(data);
      break;
    case CMD_AUDIO_GET:
      this->handle_audio_report_(data);
      break;
    case CMD_SERIAL_GET:
      this->handle_serial_report_(data);
      break;
    case CMD_TILING_GET:
      this->handle_tiling_report_(data);
      break;
    default:
      ESP_LOGV(TAG, "Unhandled report code %02X", data[0]);
      break;
  }
}

void PhilipsSicp::handle_power_report_(const std::vector<uint8_t> &data) {
  if (data.size() < 2)
    return;
  bool on = (data[1] == 0x02);
  ESP_LOGD(TAG, "Power report value=%02X on=%d switch=%s", data[1], (int) on,
           this->power_switch_ != nullptr ? "set" : "null");  if (data[1] != 0x01 && data[1] != 0x02) {
    ESP_LOGW(TAG, "Unexpected power value %02X", data[1]);
    return;
  }
  if (this->power_switch_ != nullptr)
    this->power_switch_->publish_state(on);
}

void PhilipsSicp::handle_input_report_(const std::vector<uint8_t> &data) {
  if (data.size() < 3)
    return;
  int idx = this->input_report_to_index(data[1], data[2]);
  if (idx < 0) {
    ESP_LOGW(TAG, "Unknown input report type=%02X number=%02X", data[1], data[2]);
    return;
  }
  if (this->input_select_ != nullptr)
    this->input_select_->publish_state(INPUT_OPTIONS[idx]);
}

void PhilipsSicp::handle_video_report_(const std::vector<uint8_t> &data) {
  if (data.size() < 6)
    return;
  this->brightness_ = data[1];
  this->contrast_ = data[3];
  this->sharpness_ = data[4];
  this->video_cache_valid_ = true;
  if (this->brightness_number_ != nullptr)
    this->brightness_number_->publish_state(this->brightness_);
  if (this->contrast_number_ != nullptr)
    this->contrast_number_->publish_state(this->contrast_);
  if (this->sharpness_number_ != nullptr)
    this->sharpness_number_->publish_state(this->sharpness_);
}

void PhilipsSicp::handle_picture_format_report_(const std::vector<uint8_t> &data) {
  if (data.size() < 2)
    return;
  uint8_t fmt = data[1] & 0x07;
  if (fmt >= PICTURE_FORMAT_OPTIONS_COUNT) {
    ESP_LOGW(TAG, "Unknown picture format %02X", data[1]);
    return;
  }
  if (this->picture_format_select_ != nullptr)
    this->picture_format_select_->publish_state(PICTURE_FORMAT_OPTIONS[fmt]);
}

void PhilipsSicp::handle_volume_report_(const std::vector<uint8_t> &data) {
  if (data.size() < 2)
    return;
  float v = data[1];
  if (this->volume_number_ != nullptr)
    this->volume_number_->publish_state(v);
}

void PhilipsSicp::handle_misc_report_(const std::vector<uint8_t> &data) {
  // Operating hours: report is 0F MSB LSB (GET was 0F 02).
  // Tolerate an echoed item byte: 0F 02 MSB LSB.
  uint16_t hours = 0;
  if (data.size() == 3) {
    hours = (uint16_t(data[1]) << 8) | data[2];
  } else if (data.size() >= 4 && data[1] == 0x02) {
    hours = (uint16_t(data[2]) << 8) | data[3];
  } else {
    ESP_LOGW(TAG, "Unexpected misc report length %u", (unsigned) data.size());
    return;
  }
  if (this->operating_hours_sensor_ != nullptr)
    this->operating_hours_sensor_->publish_state((float) hours);
}

void PhilipsSicp::handle_pip_source_report_(const std::vector<uint8_t> &data) {
  // Best-effort: accept {85, FD, src, ...} or {85, src}.
  int idx = -1;
  if (data.size() >= 3 && data[1] == 0xFD) {
    idx = this->input_report_to_index(0xFD, data[2]);
  } else if (data.size() == 2) {
    if (data[1] <= 0x05)
      idx = data[1];
  }
  if (idx < 0) {
    ESP_LOGW(TAG, "Unknown PIP source report");
    return;
  }
  if (this->pip_source_select_ != nullptr)
    this->pip_source_select_->publish_state(INPUT_OPTIONS[idx]);
}

void PhilipsSicp::handle_temp_report_(const std::vector<uint8_t> &data) {
  if (data.size() < 2)
    return;
  if (this->temperature_sensor_ != nullptr)
    this->temperature_sensor_->publish_state((float) data[1]);
}

void PhilipsSicp::handle_version_report_(const std::vector<uint8_t> &data) {
  if (data.size() < 2)
    return;
  std::string label(reinterpret_cast<const char *>(data.data() + 1), data.size() - 1);
  // A2 reports carry no "which label" byte, so route via the label requested
  // by the outstanding GET (tracked in last_version_label_).
  text_sensor::TextSensor *target =
      (this->last_version_label_ == 0x01) ? this->software_version_text_ : this->sicp_version_text_;
  if (target != nullptr)
    target->publish_state(label);
  else
    ESP_LOGD(TAG, "Version report with no version entity enabled: '%s'", label.c_str());
}

void PhilipsSicp::handle_input_lock_report_(const std::vector<uint8_t> &data) {
  if (data.size() < 2)
    return;
  // Documentation-derived and ambiguous: bit0 reads 1=unlocked for the remote
  // control; upper bits are marked unused, so the keyboard switch stays
  // optimistic-only and is never overwritten here.
  bool remote_unlocked = (data[1] & 0x01) != 0;
  this->remote_unlocked_ = remote_unlocked;
  if (this->remote_lock_switch_ != nullptr)
    this->remote_lock_switch_->publish_state(remote_unlocked);
}

void PhilipsSicp::handle_audio_report_(const std::vector<uint8_t> &data) {
  if (data.size() < 3)
    return;
  this->treble_ = data[1];
  this->bass_ = data[2];
  this->audio_cache_valid_ = true;
  if (this->treble_number_ != nullptr)
    this->treble_number_->publish_state(this->treble_);
  if (this->bass_number_ != nullptr)
    this->bass_number_->publish_state(this->bass_);
}

void PhilipsSicp::handle_serial_report_(const std::vector<uint8_t> &data) {
  if (data.size() < 2 || this->serial_text_ == nullptr)
    return;
  std::string code(reinterpret_cast<const char *>(data.data() + 1), data.size() - 1);
  this->serial_text_->publish_state(code);
}

void PhilipsSicp::handle_tiling_report_(const std::vector<uint8_t> &data) {
  if (data.size() < 5)
    return;
  this->tiling_enabled_ = (data[1] != 0x00);
  this->tiling_frame_ = (data[2] != 0x00);
  if (data[3] >= 0x01 && data[3] <= 0x19)
    this->tiling_position_ = data[3];
  uint8_t v = 1, h = 1;
  if (tiling_code_to_vh(data[4], &v, &h)) {
    this->tiling_v_ = v;
    this->tiling_h_ = h;
  } else {
    ESP_LOGW(TAG, "Unknown tiling V/H code %02X", data[4]);
  }
  if (this->tiling_enable_switch_ != nullptr)
    this->tiling_enable_switch_->publish_state(this->tiling_enabled_);
  if (this->tiling_frame_switch_ != nullptr)
    this->tiling_frame_switch_->publish_state(this->tiling_frame_);
  if (this->tiling_position_number_ != nullptr)
    this->tiling_position_number_->publish_state((float) this->tiling_position_);
  if (this->tiling_h_number_ != nullptr)
    this->tiling_h_number_->publish_state((float) this->tiling_h_);
  if (this->tiling_v_number_ != nullptr)
    this->tiling_v_number_->publish_state((float) this->tiling_v_);
}

// ---- High-level requests ----

void PhilipsSicp::request_power(bool on) {
  this->enqueue_command({CMD_POWER_SET, static_cast<uint8_t>(on ? 0x02 : 0x01)}, false);
  if (this->power_switch_ != nullptr)
    this->power_switch_->publish_state(on);
}

void PhilipsSicp::request_input_by_index(size_t index) {
  auto payload = this->input_set_payload(index);
  if (payload.empty())
    return;
  this->enqueue_command(payload, false);
  if (this->input_select_ != nullptr)
    this->input_select_->publish_state(INPUT_OPTIONS[index]);
}

void PhilipsSicp::request_volume(float value) {
  uint8_t v = (uint8_t) value;
  if (v > 100)
    v = 100;
  this->enqueue_command({CMD_VOLUME_SET, v}, false);
  if (this->volume_number_ != nullptr)
    this->volume_number_->publish_state((float) v);
}

void PhilipsSicp::request_picture_format_by_index(size_t index) {
  if (index >= PICTURE_FORMAT_OPTIONS_COUNT)
    return;
  this->enqueue_command({CMD_PICTURE_FORMAT_SET, static_cast<uint8_t>(index)}, false);
  if (this->picture_format_select_ != nullptr)
    this->picture_format_select_->publish_state(PICTURE_FORMAT_OPTIONS[index]);
}

void PhilipsSicp::request_pip(bool on) {
  this->pip_on_ = on;
  this->enqueue_command(
      {CMD_PIP_SET, static_cast<uint8_t>(on ? 0x01 : 0x00), this->pip_position_, 0x00, 0x00}, false);
  if (this->pip_switch_ != nullptr)
    this->pip_switch_->publish_state(on);
}

void PhilipsSicp::request_pip_position_by_index(size_t index) {
  if (index >= PIP_POSITION_OPTIONS_COUNT)
    return;
  this->pip_position_ = (uint8_t) index;
  this->enqueue_command({CMD_PIP_SET, static_cast<uint8_t>(this->pip_on_ ? 0x01 : 0x00),
                         this->pip_position_, 0x00, 0x00},
                        false);
  if (this->pip_position_select_ != nullptr)
    this->pip_position_select_->publish_state(PIP_POSITION_OPTIONS[index]);
}

void PhilipsSicp::request_pip_source_by_index(size_t index) {
  if (index >= INPUT_OPTIONS_COUNT)
    return;
  this->enqueue_command({CMD_PIP_SOURCE_SET, 0xFD, static_cast<uint8_t>(index)}, false);
  if (this->pip_source_select_ != nullptr)
    this->pip_source_select_->publish_state(INPUT_OPTIONS[index]);
}

void PhilipsSicp::request_video_param(size_t slot, float value) {
  uint8_t v = (uint8_t) value;
  if (v > 100)
    v = 100;
  if (slot == 0)
    this->brightness_ = v;
  else if (slot == 1)
    this->contrast_ = v;
  else if (slot == 2)
    this->sharpness_ = v;
  else
    return;
  this->video_cache_valid_ = true;
  this->enqueue_command({CMD_VIDEO_SET, (uint8_t) this->brightness_, 0x00, (uint8_t) this->contrast_,
                         (uint8_t) this->sharpness_, 0x00},
                        false);
  if (slot == 0 && this->brightness_number_ != nullptr)
    this->brightness_number_->publish_state((float) v);
  if (slot == 1 && this->contrast_number_ != nullptr)
    this->contrast_number_->publish_state((float) v);
  if (slot == 2 && this->sharpness_number_ != nullptr)
    this->sharpness_number_->publish_state((float) v);
}

void PhilipsSicp::request_poll_once_() {
  // Round-robin over enabled pollable entities: one GET per update() call.
  // Order is fixed; disabled entities are skipped.
  static const size_t NUM_SLOTS = 14;
  for (size_t i = 0; i < NUM_SLOTS; i++) {
    size_t slot = (this->poll_slot_ + i) % NUM_SLOTS;
    bool enqueued = true;
    switch (slot) {
      case 0:
        if (this->power_switch_ != nullptr)
          this->enqueue_command({CMD_POWER_GET}, true);
        else
          enqueued = false;
        break;
      case 1:
        if (this->input_select_ != nullptr)
          this->enqueue_command({CMD_INPUT_GET}, true);
        else
          enqueued = false;
        break;
      case 2:
        if (this->volume_number_ != nullptr)
          this->enqueue_command({CMD_VOLUME_GET}, true);
        else
          enqueued = false;
        break;
      case 3:
        if (this->picture_format_select_ != nullptr)
          this->enqueue_command({CMD_PICTURE_FORMAT_GET}, true);
        else
          enqueued = false;
        break;
      case 4:
        if (this->brightness_number_ != nullptr || this->contrast_number_ != nullptr ||
            this->sharpness_number_ != nullptr)
          this->enqueue_command({CMD_VIDEO_GET}, true);
        else
          enqueued = false;
        break;
      case 5:
        if (this->operating_hours_sensor_ != nullptr)
          this->enqueue_command({CMD_MISC_GET, 0x02}, true);
        else
          enqueued = false;
        break;
      case 6:
        if (this->temperature_sensor_ != nullptr)
          this->enqueue_command({CMD_TEMP_GET}, true);
        else
          enqueued = false;
        break;
      case 7:
        if (this->pip_source_select_ != nullptr)
          this->enqueue_command({CMD_PIP_SOURCE_GET}, true);
        else
          enqueued = false;
        break;
      case 8:
        if (this->sicp_version_text_ != nullptr) {
          this->last_version_label_ = 0x00;
          this->enqueue_command({CMD_VERSION_GET, 0x00}, true);
        } else {
          enqueued = false;
        }
        break;
      case 9:
        if (this->software_version_text_ != nullptr) {
          this->last_version_label_ = 0x01;
          this->enqueue_command({CMD_VERSION_GET, 0x01}, true);
        } else {
          enqueued = false;
        }
        break;
      case 10:
        if (this->remote_lock_switch_ != nullptr || this->keyboard_lock_switch_ != nullptr)
          this->enqueue_command({CMD_INPUT_LOCK_GET}, true);
        else
          enqueued = false;
        break;
      case 11:
        if (this->treble_number_ != nullptr || this->bass_number_ != nullptr)
          this->enqueue_command({CMD_AUDIO_GET}, true);
        else
          enqueued = false;
        break;
      case 12:
        if (this->serial_text_ != nullptr)
          this->enqueue_command({CMD_SERIAL_GET}, true);
        else
          enqueued = false;
        break;
      case 13:
        if (this->tiling_enable_switch_ != nullptr || this->tiling_frame_switch_ != nullptr ||
            this->tiling_position_number_ != nullptr || this->tiling_h_number_ != nullptr ||
            this->tiling_v_number_ != nullptr)
          this->enqueue_command({CMD_TILING_GET}, true);
        else
          enqueued = false;
        break;
      default:
        enqueued = false;
        break;
    }
    if (enqueued) {
      this->poll_slot_ = (slot + 1) % NUM_SLOTS;
      return;
    }
  }
}

void PhilipsSicp::update() { this->request_poll_once_(); }

void PhilipsSicp::request_input_lock(size_t slot, bool unlocked) {
  if (slot == 0)
    this->remote_unlocked_ = unlocked;
  else if (slot == 1)
    this->keyboard_unlocked_ = unlocked;
  else
    return;
  uint8_t v = 0;
  if (this->remote_unlocked_)
    v |= 0x01;
  if (this->keyboard_unlocked_)
    v |= 0x02;
  this->enqueue_command({CMD_INPUT_LOCK_SET, v}, false);
  // NOTE: the GET report documents only bit0, so the keyboard switch is
  // optimistic-only and never overwritten by polling.
  if (slot == 0 && this->remote_lock_switch_ != nullptr)
    this->remote_lock_switch_->publish_state(unlocked);
  if (slot == 1 && this->keyboard_lock_switch_ != nullptr)
    this->keyboard_lock_switch_->publish_state(unlocked);
}

void PhilipsSicp::request_cold_start_by_index(size_t index) {
  if (index >= COLD_START_OPTIONS_COUNT)
    return;
  this->enqueue_command({CMD_COLD_START_SET, static_cast<uint8_t>(index)}, false);
  if (this->cold_start_select_ != nullptr)
    this->cold_start_select_->publish_state(COLD_START_OPTIONS[index]);
}

void PhilipsSicp::request_volume_limit(size_t slot, float value) {
  uint8_t v = (uint8_t) value;
  if (v > 100)
    v = 100;
  if (slot == 0)
    this->min_volume_ = v;
  else if (slot == 1)
    this->max_volume_ = v;
  else if (slot == 2)
    this->switch_on_volume_ = v;
  else
    return;
  // Enforce the documented rule min <= switch-on <= max.
  if (this->switch_on_volume_ < this->min_volume_)
    this->switch_on_volume_ = this->min_volume_;
  if (this->switch_on_volume_ > this->max_volume_)
    this->switch_on_volume_ = this->max_volume_;
  this->enqueue_command({CMD_VOLUME_LIMITS_SET, (uint8_t) this->min_volume_, (uint8_t) this->max_volume_,
                         (uint8_t) this->switch_on_volume_},
                        false);
  if (this->min_volume_number_ != nullptr)
    this->min_volume_number_->publish_state(this->min_volume_);
  if (this->max_volume_number_ != nullptr)
    this->max_volume_number_->publish_state(this->max_volume_);
  if (this->switch_on_volume_number_ != nullptr)
    this->switch_on_volume_number_->publish_state(this->switch_on_volume_);
}

void PhilipsSicp::request_audio(size_t slot, float value) {
  uint8_t v = (uint8_t) value;
  if (v > 100)
    v = 100;
  if (slot == 0)
    this->treble_ = v;
  else if (slot == 1)
    this->bass_ = v;
  else
    return;
  this->audio_cache_valid_ = true;
  this->enqueue_command({CMD_AUDIO_SET, (uint8_t) this->treble_, (uint8_t) this->bass_}, false);
  if (slot == 0 && this->treble_number_ != nullptr)
    this->treble_number_->publish_state((float) v);
  if (slot == 1 && this->bass_number_ != nullptr)
    this->bass_number_->publish_state((float) v);
}

void PhilipsSicp::request_smartpower_by_index(size_t index) {
  if (index >= SMARTPOWER_OPTIONS_COUNT)
    return;
  // Payload shape follows the document's worked example ([DD, level]);
  // the field table suggests a type byte that the example omits.
  this->enqueue_command({CMD_SMARTPOWER_SET, static_cast<uint8_t>(index)}, false);
  if (this->smartpower_select_ != nullptr)
    this->smartpower_select_->publish_state(SMARTPOWER_OPTIONS[index]);
}

void PhilipsSicp::request_auto_adjust() { this->enqueue_command({CMD_AUTO_ADJUST_SET, 0x40, 0x00}, false); }

void PhilipsSicp::request_autosignal_probe() {
  // GET payload is a bare command byte; the report layout is undocumented,
  // so the reply is only logged (DEBUG) for discovery.
  this->enqueue_command({CMD_AUTOSIGNAL_GET}, true);
}

void PhilipsSicp::send_tiling_set_() {
  uint8_t vh = tiling_vh_to_code(this->tiling_v_, this->tiling_h_);
  this->enqueue_command({CMD_TILING_SET, static_cast<uint8_t>(this->tiling_enabled_ ? 0x01 : 0x00),
                         static_cast<uint8_t>(this->tiling_frame_ ? 0x01 : 0x00),
                         this->tiling_position_, vh},
                        false);
}

void PhilipsSicp::request_tiling_enable(bool on) {
  this->tiling_enabled_ = on;
  // Keep other fields: frame "don't overwrite" (0x02), position/VH keep.
  this->enqueue_command({CMD_TILING_SET, static_cast<uint8_t>(on ? 0x01 : 0x00), 0x02, 0x00, 0x00},
                        false);
  if (this->tiling_enable_switch_ != nullptr)
    this->tiling_enable_switch_->publish_state(on);
}

void PhilipsSicp::request_tiling_frame(bool on) {
  this->tiling_frame_ = on;
  this->enqueue_command({CMD_TILING_SET, static_cast<uint8_t>(this->tiling_enabled_ ? 0x01 : 0x00),
                         static_cast<uint8_t>(on ? 0x01 : 0x00), 0x00, 0x00},
                        false);
  if (this->tiling_frame_switch_ != nullptr)
    this->tiling_frame_switch_->publish_state(on);
}

void PhilipsSicp::request_tiling_geometry(size_t slot, float value) {
  if (slot == 0) {
    uint8_t p = (uint8_t) value;
    if (p < 1 || p > 25)
      return;
    this->tiling_position_ = p;
  } else if (slot == 1) {
    uint8_t h = (uint8_t) value;
    if (h < 1 || h > 5)
      return;
    this->tiling_h_ = h;
  } else if (slot == 2) {
    uint8_t v = (uint8_t) value;
    if (v < 1 || v > 5)
      return;
    this->tiling_v_ = v;
  } else {
    return;
  }
  this->send_tiling_set_();
  if (slot == 0 && this->tiling_position_number_ != nullptr)
    this->tiling_position_number_->publish_state((float) this->tiling_position_);
  if (slot == 1 && this->tiling_h_number_ != nullptr)
    this->tiling_h_number_->publish_state((float) this->tiling_h_);
  if (slot == 2 && this->tiling_v_number_ != nullptr)
    this->tiling_v_number_->publish_state((float) this->tiling_v_);
}

// ---- Child entities ----

void SicpPowerSwitch::write_state(bool state) {
  if (this->parent_ != nullptr)
    this->parent_->request_power(state);
  else
    this->publish_state(state);
}

void SicpPipSwitch::write_state(bool state) {
  if (this->parent_ != nullptr)
    this->parent_->request_pip(state);
  else
    this->publish_state(state);
}

void SicpInputSelect::control(const std::string &value) {
  if (this->parent_ == nullptr)
    return;
  for (size_t i = 0; i < PhilipsSicp::input_options_size(); i++) {
    if (value == PhilipsSicp::input_options()[i]) {
      this->parent_->request_input_by_index(i);
      return;
    }
  }
  ESP_LOGW(TAG, "Unknown input option '%s'", value.c_str());
}

void SicpPictureFormatSelect::control(const std::string &value) {
  if (this->parent_ == nullptr)
    return;
  for (size_t i = 0; i < PhilipsSicp::picture_format_options_size(); i++) {
    if (value == PhilipsSicp::picture_format_options()[i]) {
      this->parent_->request_picture_format_by_index(i);
      return;
    }
  }
  ESP_LOGW(TAG, "Unknown picture format '%s'", value.c_str());
}

void SicpPipPositionSelect::control(const std::string &value) {
  if (this->parent_ == nullptr)
    return;
  for (size_t i = 0; i < PhilipsSicp::pip_position_options_size(); i++) {
    if (value == PhilipsSicp::pip_position_options()[i]) {
      this->parent_->request_pip_position_by_index(i);
      return;
    }
  }
  ESP_LOGW(TAG, "Unknown PIP position '%s'", value.c_str());
}

void SicpPipSourceSelect::control(const std::string &value) {
  if (this->parent_ == nullptr)
    return;
  for (size_t i = 0; i < PhilipsSicp::input_options_size(); i++) {
    if (value == PhilipsSicp::input_options()[i]) {
      this->parent_->request_pip_source_by_index(i);
      return;
    }
  }
  ESP_LOGW(TAG, "Unknown PIP source '%s'", value.c_str());
}

void SicpVolumeNumber::control(float value) {
  if (this->parent_ != nullptr)
    this->parent_->request_volume(value);
  else
    this->publish_state(value);
}

void SicpVideoParamNumber::control(float value) {
  if (this->parent_ != nullptr)
    this->parent_->request_video_param(this->slot_, value);
  else
    this->publish_state(value);
}

void SicpLockSwitch::write_state(bool state) {
  if (this->parent_ != nullptr)
    this->parent_->request_input_lock(this->slot_, state);
  else
    this->publish_state(state);
}

void SicpColdStartSelect::control(const std::string &value) {
  if (this->parent_ == nullptr)
    return;
  for (size_t i = 0; i < PhilipsSicp::cold_start_options_size(); i++) {
    if (value == PhilipsSicp::cold_start_options()[i]) {
      this->parent_->request_cold_start_by_index(i);
      return;
    }
  }
  ESP_LOGW(TAG, "Unknown cold start option '%s'", value.c_str());
}

void SicpSmartPowerSelect::control(const std::string &value) {
  if (this->parent_ == nullptr)
    return;
  for (size_t i = 0; i < PhilipsSicp::smartpower_options_size(); i++) {
    if (value == PhilipsSicp::smartpower_options()[i]) {
      this->parent_->request_smartpower_by_index(i);
      return;
    }
  }
  ESP_LOGW(TAG, "Unknown SmartPower option '%s'", value.c_str());
}

void SicpAudioNumber::control(float value) {
  if (this->parent_ != nullptr)
    this->parent_->request_audio(this->slot_, value);
  else
    this->publish_state(value);
}

void SicpVolumeLimitNumber::control(float value) {
  if (this->parent_ != nullptr)
    this->parent_->request_volume_limit(this->slot_, value);
  else
    this->publish_state(value);
}

void SicpAutoAdjustButton::press_action() {
  if (this->parent_ != nullptr)
    this->parent_->request_auto_adjust();
}

void SicpAutoSignalButton::press_action() {
  if (this->parent_ != nullptr)
    this->parent_->request_autosignal_probe();
}

void SicpTilingEnableSwitch::write_state(bool state) {
  if (this->parent_ != nullptr)
    this->parent_->request_tiling_enable(state);
  else
    this->publish_state(state);
}

void SicpTilingFrameSwitch::write_state(bool state) {
  if (this->parent_ != nullptr)
    this->parent_->request_tiling_frame(state);
  else
    this->publish_state(state);
}

void SicpTilingNumber::control(float value) {
  if (this->parent_ != nullptr)
    this->parent_->request_tiling_geometry(this->slot_, value);
  else
    this->publish_state(value);
}

}  // namespace esphome::philips_sicp
