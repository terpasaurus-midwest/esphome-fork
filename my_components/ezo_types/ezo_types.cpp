#include "ezo_types.h"
#include "esphome/core/log.h"
#include "esphome/core/helpers.h"

namespace esphome {
namespace ezo_types {

static const char *const TAG = "ezo_types";

static const char *const STATUS_PREFIX = "?STATUS,";
static const char *const CAL_PREFIX = "?CAL,";

void EZOSensor::setup() {
  ezo::EZOSensor::setup();
  this->add_custom_callback([this](std::string response) { this->handle_custom_response_(response); });
  this->add_device_infomation_callback(
      [this](std::string response) { this->parse_device_information_response_(response); });

  this->send_custom("STATUS");
  this->get_device_information();
}

void EZOSensor::update() {
  ezo::EZOSensor::update();

  // Send STATUS command periodically to update voltage and reset reason
  this->send_custom("STATUS");

  // Send device info query occasionally
  static uint8_t device_info_counter = 0;
  if (++device_info_counter >= 10) {
    device_info_counter = 0;
    this->get_device_information();
  }
}

void EZOSensor::dump_config_base_(const char *sensor_type) {
  ESP_LOGCONFIG(TAG, "%s EZO Sensor:", sensor_type);
  LOG_SENSOR("", sensor_type, this);
  if (this->voltage_sensor_) {
    LOG_SENSOR("", "Voltage", this->voltage_sensor_);
  }
  if (this->reset_reason_sensor_) {
    LOG_TEXT_SENSOR("", "Reset Reason", this->reset_reason_sensor_);
  }
  if (this->firmware_version_sensor_) {
    LOG_TEXT_SENSOR("", "Firmware Version", this->firmware_version_sensor_);
  }
}

void EZOSensor::handle_common_responses_(const std::string &response) {
  if (response.rfind(STATUS_PREFIX, 0) == 0) {
    this->parse_common_status_response_(response);
    return;
  }

  if (response.rfind(CAL_PREFIX, 0) == 0) {
    this->parse_common_calibration_response_(response);
    return;
  }
}

void EZOSensor::parse_common_status_response_(const std::string &response) {
  std::string payload = response.substr(strlen(STATUS_PREFIX));
  size_t delim = payload.find(',');

  if (delim != std::string::npos) {
    std::string code = payload.substr(0, delim);
    std::string voltage_str = payload.substr(delim + 1);
    float voltage = parse_number<float>(voltage_str).value_or(0.0f);

    std::string reason;
    if (code == "P")
      reason = "Powered off";
    else if (code == "S")
      reason = "Software reset";
    else if (code == "B")
      reason = "Brown out";
    else if (code == "W")
      reason = "Watchdog";
    else if (code == "U")
      reason = "Unknown";
    else
      reason = "Unrecognized";

    if (this->reset_reason_sensor_) {
      this->reset_reason_sensor_->publish_state(reason);
    }
    if (this->voltage_sensor_) {
      this->voltage_sensor_->publish_state(voltage);
    }

    ESP_LOGI(TAG, "Last Reset: %s | Vcc: %.3f V", reason.c_str(), voltage);
  } else {
    ESP_LOGW(TAG, "Malformed status response: %s", response.c_str());
  }
}

void EZOSensor::parse_common_calibration_response_(const std::string &response) {
  std::string cal_state_str = response.substr(strlen(CAL_PREFIX));
  int cal_state = parse_number<int>(cal_state_str).value_or(0);
  ESP_LOGI(TAG, "Calibration state: %d", cal_state);
}

void EZOSensor::parse_device_information_response_(const std::string &response) {
  ESP_LOGI(TAG, "Raw device info: %s", response.c_str());

  if (this->firmware_version_sensor_) {
    // Parse device info format: "DEVICE,VERSION"
    size_t delim = response.find(',');
    if (delim != std::string::npos) {
      std::string device_type = response.substr(0, delim);
      std::string firmware_version = response.substr(delim + 1);

      this->firmware_version_sensor_->publish_state(firmware_version);
      ESP_LOGI(TAG, "Device: %s, Firmware: %s", device_type.c_str(), firmware_version.c_str());
    } else {
      // Fallback: if no comma found, publish the whole response
      this->firmware_version_sensor_->publish_state(response);
      ESP_LOGW(TAG, "Unexpected device info format: %s", response.c_str());
    }
  }
}

void PHSensor::dump_config() { this->dump_config_base_("pH"); }

void PHSensor::handle_custom_response_(const std::string &response) {
  ESP_LOGI(TAG, "[PH] Custom response: '%s'", response.c_str());

  // Try common responses first
  this->handle_common_responses_(response);
}

void ECSensor::dump_config() {
  this->dump_config_base_("EC");
  if (this->tds_sensor_) {
    LOG_SENSOR("", "TDS", this->tds_sensor_);
  }
  if (this->salinity_sensor_) {
    LOG_SENSOR("", "Salinity", this->salinity_sensor_);
  }
  if (this->relative_density_sensor_) {
    LOG_SENSOR("", "Relative Density", this->relative_density_sensor_);
  }
}

void ECSensor::handle_custom_response_(const std::string &response) {
  ESP_LOGI(TAG, "[EC] Custom response: '%s'", response.c_str());

  // Try common responses first (STATUS, CAL, etc.)
  this->handle_common_responses_(response);

  // Handle Cell Constant query response "?K,n.n"
  const std::string k_prefix = "?K,";
  if (response.rfind(k_prefix, 0) == 0) {
    this->parse_cell_constant_response_(response);
    return;
  }

  // Handle output parameters response "?O,EC,TDS"
  const std::string output_prefix = "?O,";
  if (response.rfind(output_prefix, 0) == 0) {
    this->parse_output_params_response_(response);
    return;
  }

  // Handle multi-value read response
  if (response.find(',') != std::string::npos || (response.find('.') != std::string::npos && response != "1") ||
      (response.find('.') == std::string::npos && response.length() > 0 && isdigit(response[0]))) {
    this->parse_multi_value_response_(response);
    return;
  }
}

void ECSensor::parse_cell_constant_response_(const std::string &response) {
  const std::string k_prefix = "?K,";
  std::string cell_constant_str = response.substr(k_prefix.size());
  float cell_constant = parse_number<float>(cell_constant_str).value_or(1.0f);

  this->cell_constant_callback_.call(cell_constant);
  ESP_LOGI(TAG, "[EC] Cell Constant: %.2f", cell_constant);
}

void ECSensor::parse_output_params_response_(const std::string &response) {
  const std::string output_prefix = "?O,";
  std::string params_str = response.substr(output_prefix.size());

  bool ec_enabled = params_str.find("EC") != std::string::npos;
  bool tds_enabled = params_str.find("TDS") != std::string::npos;
  bool rd_enabled = params_str.find("SG") != std::string::npos;

  // Check for standalone "S" parameter (salinity)
  bool s_enabled = (params_str.find(",S,") != std::string::npos) ||
                   (params_str.length() > 1 && params_str.substr(params_str.length() - 2) == ",S");

  this->output_params_callback_.call(ec_enabled, tds_enabled, s_enabled, rd_enabled);

  ESP_LOGI(TAG, "[EC] Output parameters - EC:%s TDS:%s S:%s RD:%s", ec_enabled ? "ON" : "OFF",
           tds_enabled ? "ON" : "OFF", s_enabled ? "ON" : "OFF", rd_enabled ? "ON" : "OFF");
}

void ECSensor::parse_multi_value_response_(const std::string &response) {
  std::vector<std::string> values;
  std::string remaining = response;
  size_t pos = 0;
  while ((pos = remaining.find(',')) != std::string::npos) {
    values.push_back(remaining.substr(0, pos));
    remaining = remaining.substr(pos + 1);
  }
  if (!remaining.empty()) {
    values.push_back(remaining);
  }

  ESP_LOGI(TAG, "[EC] Parsing %d values (ignoring EC): tds=%s s=%s rd=%s", values.size(),
           output_tds_enabled_ ? "ON" : "OFF", output_salinity_enabled_ ? "ON" : "OFF",
           output_rd_enabled_ ? "ON" : "OFF");

  // Skip EC value if present, parse remaining values in order: TDS, S, RD
  int idx = output_ec_enabled_ ? 1 : 0;

  if (output_tds_enabled_ && idx < values.size() && this->tds_sensor_) {
    float tds_val = parse_number<float>(values[idx]).value_or(0.0f);
    this->tds_sensor_->publish_state(tds_val);
    ESP_LOGI(TAG, "[EC] TDS: %.0f ppm", tds_val);
    idx++;
  }

  if (output_salinity_enabled_ && idx < values.size() && this->salinity_sensor_) {
    float s_val = parse_number<float>(values[idx]).value_or(0.0f);
    this->salinity_sensor_->publish_state(s_val);
    ESP_LOGI(TAG, "[EC] Salinity: %.2f ppt", s_val);
    idx++;
  }

  if (output_rd_enabled_ && idx < values.size() && this->relative_density_sensor_) {
    float rd_val = parse_number<float>(values[idx]).value_or(0.0f);
    this->relative_density_sensor_->publish_state(rd_val);
    ESP_LOGI(TAG, "[EC] Relative Density: %.3f", rd_val);
    idx++;
  }
}

void RTDSensor::dump_config() { this->dump_config_base_("RTD"); }

void RTDSensor::handle_custom_response_(const std::string &response) {
  ESP_LOGI(TAG, "[RTD] Custom response: '%s'", response.c_str());

  // Check if it's a temperature scale response
  std::string upper = response;
  std::transform(upper.begin(), upper.end(), upper.begin(), ::toupper);
  if (upper == "C" || upper == "F" || upper == "K") {
    this->parse_temp_scale_response_(upper);
    return;
  }

  // Handle datalogger interval response "?D,n"
  const std::string dl_prefix = "?D,";
  if (response.rfind(dl_prefix, 0) == 0) {
    this->parse_datalogger_response_(response);
    return;
  }

  // Try common class responses if nothing else returned
  this->handle_common_responses_(response);
}

void RTDSensor::parse_temp_scale_response_(const std::string &response) {
  this->temp_scale_callback_.call(response);
  ESP_LOGI(TAG, "[RTD] Temperature scale: %s", response.c_str());
}

void RTDSensor::parse_datalogger_response_(const std::string &response) {
  const std::string dl_prefix = "?D,";
  std::string interval_str = response.substr(dl_prefix.size());
  int interval = parse_number<int>(interval_str).value_or(0);

  bool enabled = (interval != 0);
  this->datalogger_callback_.call(enabled, interval);

  ESP_LOGI(TAG, "[RTD] Datalogger %s (interval %d s)", enabled ? "ENABLED" : "DISABLED", interval);
}

void ORPSensor::dump_config() { this->dump_config_base_("ORP"); }

void ORPSensor::handle_custom_response_(const std::string &response) {
  ESP_LOGI(TAG, "[ORP] Custom response: '%s'", response.c_str());

  // Handle extended-scale response "?ORPEXT,n"
  const std::string ext_prefix = "?ORPEXT,";
  if (response.rfind(ext_prefix, 0) == 0) {
    this->parse_extended_scale_response_(response);
    return;
  }

  // Try common responses
  this->handle_common_responses_(response);
}

void ORPSensor::parse_extended_scale_response_(const std::string &response) {
  const std::string ext_prefix = "?ORPEXT,";
  std::string state_str = response.substr(ext_prefix.size());
  int state = parse_number<int>(state_str).value_or(0);

  bool enabled = (state == 1);
  this->extended_scale_callback_.call(enabled);

  ESP_LOGI(TAG, "[ORP] Extended scale %s", enabled ? "ENABLED" : "DISABLED");
}

}  // namespace ezo_types
}  // namespace esphome
