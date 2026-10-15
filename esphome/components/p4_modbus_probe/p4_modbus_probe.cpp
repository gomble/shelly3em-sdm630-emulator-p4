#include "p4_modbus_probe.h"

#include <cstdio>

#include "esphome/core/application.h"
#include "esphome/core/log.h"

namespace esphome {
namespace p4_modbus_probe {

static const char *const TAG = "p4_modbus_probe";

void P4ModbusProbe::setup() {
  this->buffer_.reserve(this->max_frame_size_);
  if (this->registers_.empty()) {
    this->registers_.resize(400, 0);
  }
  // Initialize counters to 0 immediately so entities don't sit on "unknown" forever.
  if (this->request_count_sensor_ != nullptr) {
    this->request_count_sensor_->publish_state(0);
  }
  if (this->response_count_sensor_ != nullptr) {
    this->response_count_sensor_->publish_state(0);
  }
  if (this->crc_error_sensor_ != nullptr) {
    this->crc_error_sensor_->publish_state(0);
  }
  this->last_stats_publish_ms_ = millis();
}

float P4ModbusProbe::get_setup_priority() const { return setup_priority::DATA; }

void P4ModbusProbe::dump_config() {
  ESP_LOGCONFIG(TAG, "P4 Modbus Probe:");
  ESP_LOGCONFIG(TAG, "  Slave address: %u", this->slave_address_);
  ESP_LOGCONFIG(TAG, "  Register count: %u", static_cast<unsigned>(this->registers_.size()));
  ESP_LOGCONFIG(TAG, "  Frame timeout: %u ms", static_cast<unsigned>(this->frame_timeout_ms_));
  ESP_LOGCONFIG(TAG, "  Max frame size: %u", static_cast<unsigned>(this->max_frame_size_));
  ESP_LOGCONFIG(TAG, "  Stats publish interval: %u ms", static_cast<unsigned>(this->stats_interval_ms_));
}

void P4ModbusProbe::loop() {
  while (this->available()) {
    uint8_t byte;
    if (!this->read_byte(&byte)) {
      break;
    }

    if (this->buffer_.size() < this->max_frame_size_) {
      this->buffer_.push_back(byte);
    } else {
      ESP_LOGW(TAG, "Frame buffer full, flushing partial frame");
      this->flush_frame_();
      this->buffer_.push_back(byte);
    }
    this->last_byte_ms_ = millis();
  }

  if (!this->buffer_.empty() && (millis() - this->last_byte_ms_) >= this->frame_timeout_ms_) {
    this->flush_frame_();
  }

  this->maybe_publish_stats_();
}

void P4ModbusProbe::maybe_publish_stats_() {
  uint32_t now = millis();
  if (!this->stats_dirty_) {
    return;
  }
  if ((now - this->last_stats_publish_ms_) < this->stats_interval_ms_) {
    return;
  }
  this->last_stats_publish_ms_ = now;
  this->stats_dirty_ = false;
  if (this->request_count_sensor_ != nullptr) {
    this->request_count_sensor_->publish_state(this->request_count_);
  }
  if (this->response_count_sensor_ != nullptr) {
    this->response_count_sensor_->publish_state(this->response_count_);
  }
}

void P4ModbusProbe::flush_frame_() {
  if (this->buffer_.empty()) {
    return;
  }

  this->process_frame_(this->buffer_);
  this->buffer_.clear();
}

void P4ModbusProbe::process_frame_(const std::vector<uint8_t> &frame) {
  this->frame_count_++;

  // A valid Modbus RTU read request is: address(1) function(1) start(2) count(2) crc(2) = 8 bytes.
  if (frame.size() != 8) {
    return;
  }

  if (frame[0] != this->slave_address_) {
    return;  // not addressed to us, ignore silently (other devices share this bus)
  }

  uint16_t calc_crc = crc16_(frame.data(), frame.size() - 2);
  uint16_t recv_crc = static_cast<uint16_t>(frame[frame.size() - 2]) |
                       (static_cast<uint16_t>(frame[frame.size() - 1]) << 8);
  if (calc_crc != recv_crc) {
    this->crc_error_count_++;
    // CRC errors are rare and important -> publish immediately, no throttling.
    if (this->crc_error_sensor_ != nullptr) {
      this->crc_error_sensor_->publish_state(this->crc_error_count_);
    }
    ESP_LOGW(TAG, "CRC mismatch for address 0x%02X: calc=0x%04X recv=0x%04X", frame[0], calc_crc, recv_crc);
    return;
  }

  uint8_t function = frame[1];
  if (function != 0x03 && function != 0x04) {
    ESP_LOGD(TAG, "Unsupported function code 0x%02X for our address", function);
    return;
  }

  uint16_t start = (static_cast<uint16_t>(frame[2]) << 8) | frame[3];
  uint16_t count = (static_cast<uint16_t>(frame[4]) << 8) | frame[5];

  this->request_count_++;
  this->stats_dirty_ = true;

  this->send_response_(frame[0], function, start, count);
}

void P4ModbusProbe::send_response_(uint8_t address, uint8_t function, uint16_t start, uint16_t count) {
  if (count == 0 || count > 125) {
    return;
  }

  std::vector<uint8_t> resp;
  resp.reserve(3 + count * 2 + 2);
  resp.push_back(address);
  resp.push_back(function);
  resp.push_back(static_cast<uint8_t>(count * 2));

  for (uint16_t i = 0; i < count; i++) {
    uint32_t reg_addr = static_cast<uint32_t>(start) + i;
    uint16_t value = (reg_addr < this->registers_.size()) ? this->registers_[reg_addr] : 0;
    resp.push_back(static_cast<uint8_t>(value >> 8));
    resp.push_back(static_cast<uint8_t>(value & 0xFF));
  }

  uint16_t crc = crc16_(resp.data(), resp.size());
  resp.push_back(static_cast<uint8_t>(crc & 0xFF));
  resp.push_back(static_cast<uint8_t>(crc >> 8));

  this->write_array(resp.data(), resp.size());
  this->flush();

  this->response_count_++;
  this->stats_dirty_ = true;

  ESP_LOGD(TAG, "Responded addr=0x%02X func=0x%02X start=0x%04X count=%u", address, function, start, count);
}

void P4ModbusProbe::set_register(uint16_t address, uint16_t value) {
  if (address >= this->registers_.size()) {
    this->registers_.resize(address + 1, 0);
  }
  this->registers_[address] = value;
}

void P4ModbusProbe::set_register_float(uint16_t address, float value) {
  union {
    uint16_t reg[2];
    float f;
  } converter;
  converter.f = value;
  this->set_register(address, converter.reg[1]);
  this->set_register(address + 1, converter.reg[0]);
}

uint16_t P4ModbusProbe::crc16_(const uint8_t *data, size_t len) {
  uint16_t crc = 0xFFFF;
  for (size_t pos = 0; pos < len; pos++) {
    crc ^= static_cast<uint16_t>(data[pos]);
    for (int i = 8; i != 0; i--) {
      if ((crc & 0x0001) != 0) {
        crc >>= 1;
        crc ^= 0xA001;
      } else {
        crc >>= 1;
      }
    }
  }
  return crc;
}

}  // namespace p4_modbus_probe
}  // namespace esphome
