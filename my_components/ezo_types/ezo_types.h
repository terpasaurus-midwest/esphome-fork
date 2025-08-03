#pragma once

#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/text_sensor/text_sensor.h"
#include "esphome/components/ezo/ezo.h"
#include <string>
#include <vector>

namespace esphome {
namespace ezo_types {

class EZOSensor : public ezo::EZOSensor {
 public:
  void setup() override;
  void update() override;

  void set_voltage_sensor(sensor::Sensor *voltage_sensor) { voltage_sensor_ = voltage_sensor; }
  void set_reset_reason_sensor(text_sensor::TextSensor *reset_reason_sensor) {
    reset_reason_sensor_ = reset_reason_sensor;
  }
  void set_firmware_version_sensor(text_sensor::TextSensor *firmware_version_sensor) {
    firmware_version_sensor_ = firmware_version_sensor;
  }

 protected:
  void dump_config_base_(const char *sensor_type);
  void handle_common_responses_(const std::string &response);
  void parse_common_status_response_(const std::string &response);
  void parse_common_calibration_response_(const std::string &response);
  void parse_device_information_response_(const std::string &response);

  virtual void handle_custom_response_(const std::string &response) = 0;

  sensor::Sensor *voltage_sensor_{nullptr};
  text_sensor::TextSensor *reset_reason_sensor_{nullptr};
  text_sensor::TextSensor *firmware_version_sensor_{nullptr};
};

class PHSensor : public EZOSensor {
 public:
  void dump_config() override;

 protected:
  void handle_custom_response_(const std::string &response) override;
};

class ECSensor : public EZOSensor {
 public:
  void dump_config() override;

  void set_tds_sensor(sensor::Sensor *tds_sensor) { tds_sensor_ = tds_sensor; }
  void set_salinity_sensor(sensor::Sensor *salinity_sensor) { salinity_sensor_ = salinity_sensor; }
  void set_relative_density_sensor(sensor::Sensor *relative_density_sensor) {
    relative_density_sensor_ = relative_density_sensor;
  }

  void set_output_ec_enabled(bool enabled) { output_ec_enabled_ = enabled; }
  void set_output_tds_enabled(bool enabled) { output_tds_enabled_ = enabled; }
  void set_output_salinity_enabled(bool enabled) { output_salinity_enabled_ = enabled; }
  void set_output_relative_density_enabled(bool enabled) { output_rd_enabled_ = enabled; }

  void add_cell_constant_callback(std::function<void(float)> &&callback) {
    cell_constant_callback_.add(std::move(callback));
  }
  void add_output_params_callback(std::function<void(bool, bool, bool, bool)> &&callback) {
    output_params_callback_.add(std::move(callback));
  }

 protected:
  void handle_custom_response_(const std::string &response) override;
  void parse_cell_constant_response_(const std::string &response);
  void parse_output_params_response_(const std::string &response);
  void parse_multi_value_response_(const std::string &response);

  sensor::Sensor *tds_sensor_{nullptr};
  sensor::Sensor *salinity_sensor_{nullptr};
  sensor::Sensor *relative_density_sensor_{nullptr};

  bool output_ec_enabled_{true};
  bool output_tds_enabled_{false};
  bool output_salinity_enabled_{false};
  bool output_rd_enabled_{false};

  CallbackManager<void(float)> cell_constant_callback_{};
  CallbackManager<void(bool, bool, bool, bool)> output_params_callback_{};
};

class RTDSensor : public EZOSensor {
 public:
  void dump_config() override;

  void add_temp_scale_callback(std::function<void(std::string)> &&callback) {
    temp_scale_callback_.add(std::move(callback));
  }
  void add_datalogger_callback(std::function<void(bool, int)> &&callback) {
    datalogger_callback_.add(std::move(callback));
  }

 protected:
  void handle_custom_response_(const std::string &response) override;
  void parse_temp_scale_response_(const std::string &response);
  void parse_datalogger_response_(const std::string &response);

  CallbackManager<void(std::string)> temp_scale_callback_{};
  CallbackManager<void(bool, int)> datalogger_callback_{};
};

class ORPSensor : public EZOSensor {
 public:
  void dump_config() override;

  void add_extended_scale_callback(std::function<void(bool)> &&callback) {
    extended_scale_callback_.add(std::move(callback));
  }

 protected:
  void handle_custom_response_(const std::string &response) override;
  void parse_extended_scale_response_(const std::string &response);

  CallbackManager<void(bool)> extended_scale_callback_{};
};

}  // namespace ezo_types
}  // namespace esphome
