#include "esphome/core/log.h"
#include "esphome/core/hal.h"
#include "m5unit_co2l.h"
#include <M5Unified.h>
#include <cmath>

namespace esphome {
namespace m5unit_co2l {

static const char *TAG = "m5unit_co2l";

float M5UnitCO2L::get_setup_priority() const { return setup_priority::DATA; }

void M5UnitCO2L::setup() {
  ESP_LOGCONFIG(TAG, "Setting up M5Unit CO2L...");

  // Get I2C pins from M5Unified
  auto pin_num_sda = M5.getPin(m5::pin_name_t::port_a_sda);
  auto pin_num_scl = M5.getPin(m5::pin_name_t::port_a_scl);
  ESP_LOGCONFIG(TAG, "I2C pins - SDA: %u, SCL: %u", pin_num_sda, pin_num_scl);

  // Initialize I2C
  Wire.end();
  Wire.begin(pin_num_sda, pin_num_scl, 400000U);

  // Initialize M5Unit system
  if (!units_.add(unit_, Wire) || !units_.begin()) {
    ESP_LOGE(TAG, "Failed to initialize M5Unit CO2L");
    this->mark_failed();
    return;
  }

  ESP_LOGCONFIG(TAG, "M5UnitUnified initialized");
  ESP_LOGCONFIG(TAG, "%s", units_.debugInfo().c_str());

  // Configure CO2L sensor
  bool config_ok = true;

  // Stop any existing periodic measurements
  config_ok &= unit_.stopPeriodicMeasurement();

  // Read current configuration
  float temp_offset = 0.0f;
  uint16_t altitude = 0;
  uint16_t pressure = 0;
  bool asc_enabled = false;
  uint16_t asc_target = 0;
  uint16_t initial_period = 0;
  uint16_t standard_period = 0;

  config_ok &= unit_.readTemperatureOffset(temp_offset);
  config_ok &= unit_.readSensorAltitude(altitude);
  config_ok &= unit_.readAmbientPressure(pressure);
  config_ok &= unit_.readAutomaticSelfCalibrationEnabled(asc_enabled);
  config_ok &= unit_.readAutomaticSelfCalibrationTarget(asc_target);
  config_ok &= unit_.readAutomaticSelfCalibrationInitialPeriod(initial_period);
  config_ok &= unit_.readAutomaticSelfCalibrationStandardPeriod(standard_period);

  ESP_LOGCONFIG(TAG, "CO2L Configuration:");
  ESP_LOGCONFIG(TAG, "  Temperature offset: %.2f°C", temp_offset);
  ESP_LOGCONFIG(TAG, "  Sensor altitude: %u m", altitude);
  ESP_LOGCONFIG(TAG, "  Ambient pressure: %u Pa", pressure);
  ESP_LOGCONFIG(TAG, "  ASC enabled: %s", asc_enabled ? "YES" : "NO");
  ESP_LOGCONFIG(TAG, "  ASC target: %u ppm", asc_target);
  ESP_LOGCONFIG(TAG, "  Initial period: %u h", initial_period);
  ESP_LOGCONFIG(TAG, "  Standard period: %u h", standard_period);

  // Start periodic measurements
  config_ok &= unit_.startPeriodicMeasurement();

  if (!config_ok) {
    ESP_LOGE(TAG, "Failed to configure CO2L sensor");
    this->mark_failed();
    return;
  }

  initialized_ = true;
  ESP_LOGCONFIG(TAG, "M5Unit CO2L setup complete");
}

void M5UnitCO2L::loop() {
  if (!initialized_) {
    return;
  }

  units_.update();

  if (unit_.updated()) {
    float co2 = static_cast<float>(unit_.co2());
    float temperature = unit_.temperature();
    float humidity = unit_.humidity();

    ESP_LOGV(TAG, "CO2L readings - CO2: %.0f ppm, Temp: %.2f°C, Humidity: %.2f%%", co2, temperature, humidity);

    // Publish sensor values
    if (co2_sensor_ != nullptr) {
      co2_sensor_->publish_state(co2);
    }

    if (temperature_sensor_ != nullptr) {
      temperature_sensor_->publish_state(temperature);
    }

    if (humidity_sensor_ != nullptr) {
      humidity_sensor_->publish_state(humidity);
    }

    // Calculate and publish VPD
    if (vpd_sensor_ != nullptr) {
      calculate_and_publish_vpd_(temperature, humidity);
    }
  }
}

void M5UnitCO2L::calculate_and_publish_vpd_(float temperature, float humidity) {
  if (std::isnan(temperature) || std::isnan(humidity)) {
    ESP_LOGW(TAG, "Cannot calculate VPD: invalid temperature or humidity");
    return;
  }

  // Calculate saturation vapor pressure using Magnus formula
  float svp = 0.61078f * std::exp((17.27f * temperature) / (temperature + 237.3f));

  // Calculate vapor pressure deficit
  float vpd = svp * (1.0f - (humidity / 100.0f));

  ESP_LOGV(TAG, "VPD calculation - SVP: %.3f kPa, VPD: %.3f kPa", svp, vpd);

  vpd_sensor_->publish_state(vpd);
}

void M5UnitCO2L::dump_config() {
  ESP_LOGCONFIG(TAG, "M5Unit CO2L:");
  ESP_LOGCONFIG(TAG, "  Status: %s", initialized_ ? "OK" : "FAILED");

  if (co2_sensor_ != nullptr) {
    ESP_LOGCONFIG(TAG, "  CO2 Sensor: %s", co2_sensor_->get_name().c_str());
  }

  if (temperature_sensor_ != nullptr) {
    ESP_LOGCONFIG(TAG, "  Temperature Sensor: %s", temperature_sensor_->get_name().c_str());
  }

  if (humidity_sensor_ != nullptr) {
    ESP_LOGCONFIG(TAG, "  Humidity Sensor: %s", humidity_sensor_->get_name().c_str());
  }

  if (vpd_sensor_ != nullptr) {
    ESP_LOGCONFIG(TAG, "  VPD Sensor: %s", vpd_sensor_->get_name().c_str());
  }
}

// Calibration methods
void M5UnitCO2L::perform_forced_calibration(uint16_t target_ppm) {
  if (!initialized_) {
    ESP_LOGE(TAG, "Cannot perform forced calibration - sensor not initialized");
    return;
  }

  ESP_LOGI(TAG, "Performing forced calibration to %u ppm", target_ppm);

  // Stop periodic measurements
  if (!unit_.stopPeriodicMeasurement()) {
    ESP_LOGE(TAG, "Failed to stop periodic measurement for calibration");
    return;
  }

  // Wait for stop command to complete
  delay(500);

  // Perform forced calibration
  int16_t correction = 0;
  if (unit_.performForcedRecalibration(target_ppm, correction)) {
    ESP_LOGI(TAG, "Forced calibration successful, correction: %d ppm", correction);
  } else {
    ESP_LOGE(TAG, "Forced calibration failed");
  }

  // Restart periodic measurements
  if (!unit_.startPeriodicMeasurement()) {
    ESP_LOGE(TAG, "Failed to restart periodic measurement after calibration");
  }
}

void M5UnitCO2L::factory_reset() {
  if (!initialized_) {
    ESP_LOGE(TAG, "Cannot perform factory reset - sensor not initialized");
    return;
  }

  ESP_LOGI(TAG, "Performing factory reset");

  // Stop periodic measurements
  if (!unit_.stopPeriodicMeasurement()) {
    ESP_LOGE(TAG, "Failed to stop periodic measurement for factory reset");
    return;
  }

  // Wait for stop command to complete
  delay(500);

  // Perform factory reset
  if (unit_.performFactoryReset()) {
    ESP_LOGI(TAG, "Factory reset successful");
  } else {
    ESP_LOGE(TAG, "Factory reset failed");
  }

  // Wait for reset to complete
  delay(1200);

  // Restart periodic measurements
  if (!unit_.startPeriodicMeasurement()) {
    ESP_LOGE(TAG, "Failed to restart periodic measurement after factory reset");
  }
}

void M5UnitCO2L::set_automatic_self_calibration(bool enabled) {
  if (!initialized_) {
    ESP_LOGE(TAG, "Cannot set ASC - sensor not initialized");
    return;
  }

  ESP_LOGI(TAG, "Setting automatic self calibration: %s", enabled ? "enabled" : "disabled");

  // Stop periodic measurements
  if (!unit_.stopPeriodicMeasurement()) {
    ESP_LOGE(TAG, "Failed to stop periodic measurement for ASC setting");
    return;
  }

  // Wait for stop command to complete
  delay(500);

  // Set ASC
  if (unit_.writeAutomaticSelfCalibrationEnabled(enabled)) {
    ESP_LOGI(TAG, "ASC setting successful");
  } else {
    ESP_LOGE(TAG, "ASC setting failed");
  }

  // Restart periodic measurements
  if (!unit_.startPeriodicMeasurement()) {
    ESP_LOGE(TAG, "Failed to restart periodic measurement after ASC setting");
  }
}

bool M5UnitCO2L::get_automatic_self_calibration_enabled() {
  if (!initialized_) {
    ESP_LOGE(TAG, "Cannot get ASC status - sensor not initialized");
    return false;
  }

  // Stop periodic measurements
  if (!unit_.stopPeriodicMeasurement()) {
    ESP_LOGE(TAG, "Failed to stop periodic measurement for ASC reading");
    return false;
  }

  // Wait for stop command to complete
  delay(500);

  // Get ASC status
  bool enabled = false;
  bool success = unit_.readAutomaticSelfCalibrationEnabled(enabled);

  // Restart periodic measurements
  if (!unit_.startPeriodicMeasurement()) {
    ESP_LOGE(TAG, "Failed to restart periodic measurement after ASC reading");
  }

  return success ? enabled : false;
}

// Configuration methods
void M5UnitCO2L::set_temperature_offset(float offset) {
  if (!initialized_) {
    ESP_LOGE(TAG, "Cannot set temperature offset - sensor not initialized");
    return;
  }

  ESP_LOGI(TAG, "Setting temperature offset: %.2f°C", offset);

  // Stop periodic measurements
  if (!unit_.stopPeriodicMeasurement()) {
    ESP_LOGE(TAG, "Failed to stop periodic measurement for temperature offset");
    return;
  }

  // Wait for stop command to complete
  delay(500);

  // Set temperature offset
  if (unit_.writeTemperatureOffset(offset)) {
    ESP_LOGI(TAG, "Temperature offset setting successful");
  } else {
    ESP_LOGE(TAG, "Temperature offset setting failed");
  }

  // Restart periodic measurements
  if (!unit_.startPeriodicMeasurement()) {
    ESP_LOGE(TAG, "Failed to restart periodic measurement after temperature offset");
  }
}

float M5UnitCO2L::get_temperature_offset() {
  if (!initialized_) {
    ESP_LOGE(TAG, "Cannot get temperature offset - sensor not initialized");
    return 0.0f;
  }

  // Stop periodic measurements
  if (!unit_.stopPeriodicMeasurement()) {
    ESP_LOGE(TAG, "Failed to stop periodic measurement for temperature offset reading");
    return 0.0f;
  }

  // Wait for stop command to complete
  delay(500);

  // Get temperature offset
  float offset = 0.0f;
  unit_.readTemperatureOffset(offset);

  // Restart periodic measurements
  if (!unit_.startPeriodicMeasurement()) {
    ESP_LOGE(TAG, "Failed to restart periodic measurement after temperature offset reading");
  }

  return offset;
}

void M5UnitCO2L::set_sensor_altitude(uint16_t altitude) {
  if (!initialized_) {
    ESP_LOGE(TAG, "Cannot set sensor altitude - sensor not initialized");
    return;
  }

  ESP_LOGI(TAG, "Setting sensor altitude: %u m", altitude);

  // Stop periodic measurements
  if (!unit_.stopPeriodicMeasurement()) {
    ESP_LOGE(TAG, "Failed to stop periodic measurement for altitude setting");
    return;
  }

  // Wait for stop command to complete
  delay(500);

  // Set sensor altitude
  if (unit_.writeSensorAltitude(altitude)) {
    ESP_LOGI(TAG, "Sensor altitude setting successful");
  } else {
    ESP_LOGE(TAG, "Sensor altitude setting failed");
  }

  // Restart periodic measurements
  if (!unit_.startPeriodicMeasurement()) {
    ESP_LOGE(TAG, "Failed to restart periodic measurement after altitude setting");
  }
}

uint16_t M5UnitCO2L::get_sensor_altitude() {
  if (!initialized_) {
    ESP_LOGE(TAG, "Cannot get sensor altitude - sensor not initialized");
    return 0;
  }

  // Stop periodic measurements
  if (!unit_.stopPeriodicMeasurement()) {
    ESP_LOGE(TAG, "Failed to stop periodic measurement for altitude reading");
    return 0;
  }

  // Wait for stop command to complete
  delay(500);

  // Get sensor altitude
  uint16_t altitude = 0;
  unit_.readSensorAltitude(altitude);

  // Restart periodic measurements
  if (!unit_.startPeriodicMeasurement()) {
    ESP_LOGE(TAG, "Failed to restart periodic measurement after altitude reading");
  }

  return altitude;
}

void M5UnitCO2L::set_ambient_pressure(float pressure) {
  if (!initialized_) {
    ESP_LOGE(TAG, "Cannot set ambient pressure - sensor not initialized");
    return;
  }

  ESP_LOGI(TAG, "Setting ambient pressure: %.2f Pa", pressure);

  // Ambient pressure can be set during periodic measurements
  if (unit_.writeAmbientPressure(static_cast<uint16_t>(pressure))) {
    ESP_LOGI(TAG, "Ambient pressure setting successful");
  } else {
    ESP_LOGE(TAG, "Ambient pressure setting failed");
  }
}

void M5UnitCO2L::persist_settings() {
  if (!initialized_) {
    ESP_LOGE(TAG, "Cannot persist settings - sensor not initialized");
    return;
  }

  ESP_LOGI(TAG, "Persisting settings to EEPROM");

  // Stop periodic measurements
  if (!unit_.stopPeriodicMeasurement()) {
    ESP_LOGE(TAG, "Failed to stop periodic measurement for settings persistence");
    return;
  }

  // Wait for stop command to complete
  delay(500);

  // Persist settings
  if (unit_.writePersistSettings()) {
    ESP_LOGI(TAG, "Settings persistence successful");
  } else {
    ESP_LOGE(TAG, "Settings persistence failed");
  }

  // Restart periodic measurements
  if (!unit_.startPeriodicMeasurement()) {
    ESP_LOGE(TAG, "Failed to restart periodic measurement after settings persistence");
  }
}

// Measurement control methods
void M5UnitCO2L::start_low_power_mode() {
  if (!initialized_) {
    ESP_LOGE(TAG, "Cannot start low power mode - sensor not initialized");
    return;
  }

  ESP_LOGI(TAG, "Starting low power periodic measurement (30s interval)");

  // Stop current measurements
  if (!unit_.stopPeriodicMeasurement()) {
    ESP_LOGE(TAG, "Failed to stop periodic measurement for low power mode");
    return;
  }

  // Wait for stop command to complete
  delay(500);

  // Start low power measurements
  if (unit_.startLowPowerPeriodicMeasurement()) {
    ESP_LOGI(TAG, "Low power mode started successfully");
  } else {
    ESP_LOGE(TAG, "Low power mode start failed");
  }
}

void M5UnitCO2L::perform_single_shot() {
  if (!initialized_) {
    ESP_LOGE(TAG, "Cannot perform single shot - sensor not initialized");
    return;
  }

  ESP_LOGI(TAG, "Performing single shot measurement");

  // Stop periodic measurements
  if (!unit_.stopPeriodicMeasurement()) {
    ESP_LOGE(TAG, "Failed to stop periodic measurement for single shot");
    return;
  }

  // Wait for stop command to complete
  delay(500);

  // Perform single shot
  m5::unit::scd4x::Data data;
  if (unit_.measureSingleshot(data)) {
    ESP_LOGI(TAG, "Single shot measurement successful - CO2: %u ppm, Temp: %.2f°C, Humidity: %.2f%%", data.co2(),
             data.temperature(), data.humidity());
  } else {
    ESP_LOGE(TAG, "Single shot measurement failed");
  }

  // Restart periodic measurements
  if (!unit_.startPeriodicMeasurement()) {
    ESP_LOGE(TAG, "Failed to restart periodic measurement after single shot");
  }
}

bool M5UnitCO2L::get_data_ready_status() {
  if (!initialized_) {
    return false;
  }

  // Note: getDataReadyStatus() is not available in M5Unit-ENV library
  // The method exists as protected read_data_ready_status()
  ESP_LOGW(TAG, "get_data_ready_status() not implemented - method not available in M5Unit-ENV library");
  return false;
}

// Diagnostics methods
std::string M5UnitCO2L::get_serial_number() {
  if (!initialized_) {
    ESP_LOGE(TAG, "Cannot get serial number - sensor not initialized");
    return "";
  }

  // Stop periodic measurements
  if (!unit_.stopPeriodicMeasurement()) {
    ESP_LOGE(TAG, "Failed to stop periodic measurement for serial number reading");
    return "";
  }

  // Wait for stop command to complete
  delay(500);

  // Get serial number
  char serial[13];
  bool success = unit_.readSerialNumber(serial);

  // Restart periodic measurements
  if (!unit_.startPeriodicMeasurement()) {
    ESP_LOGE(TAG, "Failed to restart periodic measurement after serial number reading");
  }

  if (success) {
    return std::string(serial);
  } else {
    return "";
  }
}

bool M5UnitCO2L::perform_self_test() {
  if (!initialized_) {
    ESP_LOGE(TAG, "Cannot perform self test - sensor not initialized");
    return false;
  }

  ESP_LOGI(TAG, "Performing self test (10 seconds)");

  // Stop periodic measurements
  if (!unit_.stopPeriodicMeasurement()) {
    ESP_LOGE(TAG, "Failed to stop periodic measurement for self test");
    return false;
  }

  // Wait for stop command to complete
  delay(500);

  // Perform self test
  bool malfunction = false;
  bool success = unit_.performSelfTest(malfunction);

  // Restart periodic measurements
  if (!unit_.startPeriodicMeasurement()) {
    ESP_LOGE(TAG, "Failed to restart periodic measurement after self test");
  }

  if (success) {
    if (malfunction) {
      ESP_LOGE(TAG, "Self test completed but malfunction detected");
    } else {
      ESP_LOGI(TAG, "Self test passed");
    }
  } else {
    ESP_LOGE(TAG, "Self test failed");
  }

  return success && !malfunction;
}

}  // namespace m5unit_co2l
}  // namespace esphome
