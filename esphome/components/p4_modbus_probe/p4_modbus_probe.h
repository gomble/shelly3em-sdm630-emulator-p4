#pragma once

#include <vector>

#include "esphome/components/sensor/sensor.h"
#include "esphome/components/text_sensor/text_sensor.h"
#include "esphome/components/uart/uart.h"
#include "esphome/core/component.h"

namespace esphome {
namespace p4_modbus_probe {

class P4ModbusProbe : public Component, public uart::UARTDevice {
 public:
  void set_frame_timeout_ms(uint32_t timeout_ms) { this->frame_timeout_ms_ = timeout_ms; }
  void set_max_frame_size(size_t max_frame_size) { this->max_frame_size_ = max_frame_size; }
  void set_slave_address(uint8_t address) { this->slave_address_ = address; }
  void set_register_count(uint16_t count) { this->registers_.resize(count, 0); }
  void set_stats_interval_ms(uint32_t interval_ms) { this->stats_interval_ms_ = interval_ms; }

  void set_request_count_sensor(sensor::Sensor *sensor) { this->request_count_sensor_ = sensor; }
  void set_response_count_sensor(sensor::Sensor *sensor) { this->response_count_sensor_ = sensor; }
  void set_crc_error_sensor(sensor::Sensor *sensor) { this->crc_error_sensor_ = sensor; }

  // Writes a single 16-bit register value.
  void set_register(uint16_t address, uint16_t value);
  // Writes a 32-bit float across two registers (high word first), matching
  // the SDM630/Eastron register layout used by the original fake-eastron emulator.
  void set_register_float(uint16_t address, float value);

  void setup() override;
  void loop() override;
  float get_setup_priority() const override;
  void dump_config() override;

 protected:
  void flush_frame_();
  void process_frame_(const std::vector<uint8_t> &frame);
  void send_response_(uint8_t address, uint8_t function, uint16_t start, uint16_t count);
  void maybe_publish_stats_();
  static uint16_t crc16_(const uint8_t *data, size_t len);

  uint32_t frame_timeout_ms_{5};
  size_t max_frame_size_{256};
  uint32_t last_byte_ms_{0};
  std::vector<uint8_t> buffer_{};
  uint32_t frame_count_{0};
  uint32_t request_count_{0};
  uint32_t response_count_{0};
  uint32_t crc_error_count_{0};
  uint8_t slave_address_{2};
  std::vector<uint16_t> registers_;

  uint32_t stats_interval_ms_{60000};
  uint32_t last_stats_publish_ms_{0};
  bool stats_dirty_{false};

  sensor::Sensor *request_count_sensor_{nullptr};
  sensor::Sensor *response_count_sensor_{nullptr};
  sensor::Sensor *crc_error_sensor_{nullptr};
};

}  // namespace p4_modbus_probe
}  // namespace esphome
