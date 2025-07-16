#pragma once

#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/core/automation.h"
#include <M5UnitUnified.h>
#include <M5UnitUnifiedENV.h>

namespace esphome {
namespace m5unit_co2l {

class M5UnitCO2L : public Component {
 public:
  void setup() override;
  void loop() override;
  void dump_config() override;
  float get_setup_priority() const override;

  void set_co2_sensor(sensor::Sensor *co2_sensor) { co2_sensor_ = co2_sensor; }
  void set_temperature_sensor(sensor::Sensor *temperature_sensor) { temperature_sensor_ = temperature_sensor; }
  void set_humidity_sensor(sensor::Sensor *humidity_sensor) { humidity_sensor_ = humidity_sensor; }
  void set_vpd_sensor(sensor::Sensor *vpd_sensor) { vpd_sensor_ = vpd_sensor; }

  // Calibration actions
  void perform_forced_calibration(uint16_t target_ppm);
  void factory_reset();
  void set_automatic_self_calibration(bool enabled);
  bool get_automatic_self_calibration_enabled();

  // Configuration actions
  void set_temperature_offset(float offset);
  float get_temperature_offset();
  void set_sensor_altitude(uint16_t altitude);
  uint16_t get_sensor_altitude();
  void set_ambient_pressure(float pressure);
  void persist_settings();

  // Measurement control
  void start_low_power_mode();
  void perform_single_shot();
  bool get_data_ready_status();

  // Diagnostics
  std::string get_serial_number();
  bool perform_self_test();

  // Make unit accessible for display component
  m5::unit::UnitCO2L unit_;

 protected:
  void calculate_and_publish_vpd_(float temperature, float humidity);

  m5::unit::UnitUnified units_;
  bool initialized_{false};

  sensor::Sensor *co2_sensor_{nullptr};
  sensor::Sensor *temperature_sensor_{nullptr};
  sensor::Sensor *humidity_sensor_{nullptr};
  sensor::Sensor *vpd_sensor_{nullptr};
};

// Action templates for ESPHome
template<typename... Ts> class M5UnitCO2LPerformForcedCalibrationAction : public Action<Ts...> {
 public:
  M5UnitCO2LPerformForcedCalibrationAction(M5UnitCO2L *parent) : parent_(parent) {}

  TEMPLATABLE_VALUE(uint16_t, target_ppm)

  void play(Ts... x) override {
    auto target = this->target_ppm_.value(x...);
    this->parent_->perform_forced_calibration(target);
  }

 protected:
  M5UnitCO2L *parent_;
};

template<typename... Ts> class M5UnitCO2LFactoryResetAction : public Action<Ts...> {
 public:
  M5UnitCO2LFactoryResetAction(M5UnitCO2L *parent) : parent_(parent) {}

  void play(Ts... x) override { this->parent_->factory_reset(); }

 protected:
  M5UnitCO2L *parent_;
};

template<typename... Ts> class M5UnitCO2LSetTemperatureOffsetAction : public Action<Ts...> {
 public:
  M5UnitCO2LSetTemperatureOffsetAction(M5UnitCO2L *parent) : parent_(parent) {}

  TEMPLATABLE_VALUE(float, offset)

  void play(Ts... x) override {
    auto offset_val = this->offset_.value(x...);
    this->parent_->set_temperature_offset(offset_val);
  }

 protected:
  M5UnitCO2L *parent_;
};

template<typename... Ts> class M5UnitCO2LSetAutomaticSelfCalibrationAction : public Action<Ts...> {
 public:
  M5UnitCO2LSetAutomaticSelfCalibrationAction(M5UnitCO2L *parent) : parent_(parent) {}

  TEMPLATABLE_VALUE(bool, enabled)

  void play(Ts... x) override {
    auto enabled_val = this->enabled_.value(x...);
    this->parent_->set_automatic_self_calibration(enabled_val);
  }

 protected:
  M5UnitCO2L *parent_;
};

}  // namespace m5unit_co2l
}  // namespace esphome