#include "grow_env_monitor.h"
#include "esphome/core/log.h"
#include "esphome/components/web_server_base/web_server_base.h"
#include <cmath>

namespace esphome {
namespace grow_env_monitor {

static const char *TAG = "grow_env_monitor";

void GrowEnvMonitor::setup() {
  ESP_LOGCONFIG(TAG, "Setting up Grow Environment Monitor...");

  display_.begin();
  display_.setRotation(display_rotation_);
  display_.setBrightness(display_brightness_);
  display_.fillScreen(COLOR_BACKGROUND);

  // Initialize thermal camera
  setup_thermal_();

  // Draw initial screen layout and labels
  draw_screen_();

  initialized_ = true;
  ESP_LOGCONFIG(TAG, "Grow Environment Monitor setup complete");
}

void GrowEnvMonitor::loop() {
  if (!initialized_)
    return;

  uint32_t now = millis();

  // Update thermal data and image more frequently
  if (thermal_initialized_ && now - last_thermal_update_time_ > thermal_update_interval_) {
    update_thermal_data_();
    // Only redraw the thermal image area, not the whole screen
    draw_thermal_image_();
    last_thermal_update_time_ = now;
  }

  // Check for sensor value changes and selectively update display areas
  check_and_update_sensor_values_();
}

void GrowEnvMonitor::dump_config() {
  ESP_LOGCONFIG(TAG, "Grow Environment Monitor:");
  ESP_LOGCONFIG(TAG, "  Display Brightness: %d", display_brightness_);
  ESP_LOGCONFIG(TAG, "  Display Rotation: %d", display_rotation_);
  ESP_LOGCONFIG(TAG, "  Sensors configured: %s",
                (co2_sensor_ && temperature_sensor_ && humidity_sensor_) ? "Yes" : "No");
}

void GrowEnvMonitor::draw_screen_() {
  display_.fillScreen(COLOR_BACKGROUND);

  // Draw header
  display_.setTextColor(COLOR_HEADER);
  display_.setTextSize(2);
  display_.setCursor(MARGIN, MARGIN);
  display_.println("Grow Monitor");

  // Draw main sensor data
  draw_sensor_data_();

  // Draw alerts/status
  draw_alerts_();

  // Note: thermal image drawn separately at higher frequency
}

void GrowEnvMonitor::draw_sensor_data_() {
  int y = MARGIN + 30;

  display_.setTextSize(1);
  display_.setTextColor(COLOR_TEXT);

  // CO2 Data
  if (co2_sensor_ && co2_sensor_->has_state()) {
    float co2 = co2_sensor_->state;
    uint16_t co2_color = (co2 > 1500) ? COLOR_ALERT : (co2 > 1000) ? COLOR_WARNING : COLOR_GOOD;

    display_.setCursor(MARGIN, y);
    display_.setTextColor(COLOR_TEXT);
    display_.print("CO2: ");
    display_.setTextColor(co2_color);
    display_.print(co2, 0);
    display_.setTextColor(COLOR_TEXT);
    display_.println(" ppm");
  } else {
    display_.setCursor(MARGIN, y);
    display_.setTextColor(COLOR_WARNING);
    display_.println("CO2: No data");
  }
  y += LINE_HEIGHT;

  // Temperature Data
  if (temperature_sensor_ && temperature_sensor_->has_state()) {
    float temp = temperature_sensor_->state;
    uint16_t temp_color = (temp > 30 || temp < 18)   ? COLOR_ALERT
                          : (temp > 28 || temp < 20) ? COLOR_WARNING
                                                     : COLOR_GOOD;

    display_.setCursor(MARGIN, y);
    display_.setTextColor(COLOR_TEXT);
    display_.print("Temp: ");
    display_.setTextColor(temp_color);
    display_.print(temp, 1);
    display_.setTextColor(COLOR_TEXT);
    display_.println(" C");
  } else {
    display_.setCursor(MARGIN, y);
    display_.setTextColor(COLOR_WARNING);
    display_.println("Temp: No data");
  }
  y += LINE_HEIGHT;

  // Humidity Data
  if (humidity_sensor_ && humidity_sensor_->has_state()) {
    float humidity = humidity_sensor_->state;
    uint16_t humidity_color = (humidity > 70 || humidity < 40)   ? COLOR_ALERT
                              : (humidity > 65 || humidity < 45) ? COLOR_WARNING
                                                                 : COLOR_GOOD;

    display_.setCursor(MARGIN, y);
    display_.setTextColor(COLOR_TEXT);
    display_.print("Humidity: ");
    display_.setTextColor(humidity_color);
    display_.print(humidity, 1);
    display_.setTextColor(COLOR_TEXT);
    display_.println(" %");
  } else {
    display_.setCursor(MARGIN, y);
    display_.setTextColor(COLOR_WARNING);
    display_.println("Humidity: No data");
  }
  y += LINE_HEIGHT;

  // VPD Calculation
  if (temperature_sensor_ && temperature_sensor_->has_state() && humidity_sensor_ && humidity_sensor_->has_state()) {
    float vpd = calculate_vpd_(temperature_sensor_->state, humidity_sensor_->state);
    uint16_t vpd_color = (vpd > 1.5 || vpd < 0.4) ? COLOR_ALERT : (vpd > 1.2 || vpd < 0.6) ? COLOR_WARNING : COLOR_GOOD;

    display_.setCursor(MARGIN, y);
    display_.setTextColor(COLOR_TEXT);
    display_.print("VPD: ");
    display_.setTextColor(vpd_color);
    display_.print(vpd, 2);
    display_.setTextColor(COLOR_TEXT);
    display_.println(" kPa");
  } else {
    display_.setCursor(MARGIN, y);
    display_.setTextColor(COLOR_WARNING);
    display_.println("VPD: No data");
  }
}

void GrowEnvMonitor::draw_thermal_data_() {
  int y = MARGIN + 135;  // Moved up 15 pixels to prevent bottom cutoff

  display_.setTextSize(1);
  display_.setTextColor(COLOR_HEADER);
  display_.setCursor(MARGIN, y);
  display_.println("Thermal Camera");
  y += LINE_HEIGHT;

  // Thermal Min
  if (thermal_min_sensor_ && thermal_min_sensor_->has_state()) {
    display_.setCursor(MARGIN, y);
    display_.setTextColor(COLOR_TEXT);
    display_.print("Min: ");
    display_.print(thermal_min_sensor_->state, 1);
    display_.println(" C");
  } else {
    display_.setCursor(MARGIN, y);
    display_.setTextColor(COLOR_WARNING);
    display_.println("Min: No data");
  }
  y += LINE_HEIGHT;

  // Thermal Max
  if (thermal_max_sensor_ && thermal_max_sensor_->has_state()) {
    display_.setCursor(MARGIN, y);
    display_.setTextColor(COLOR_TEXT);
    display_.print("Max: ");
    display_.print(thermal_max_sensor_->state, 1);
    display_.println(" C");
  } else {
    display_.setCursor(MARGIN, y);
    display_.setTextColor(COLOR_WARNING);
    display_.println("Max: No data");
  }

  // Thermal Average (on same line as Max)
  if (thermal_avg_sensor_ && thermal_avg_sensor_->has_state()) {
    display_.setCursor(MARGIN + 120, y);
    display_.setTextColor(COLOR_TEXT);
    display_.print("Avg: ");
    display_.print(thermal_avg_sensor_->state, 1);
    display_.println(" C");
  } else {
    display_.setCursor(MARGIN + 120, y);
    display_.setTextColor(COLOR_WARNING);
    display_.println("Avg: No data");
  }
  y += LINE_HEIGHT;

  // Show temperature range and median (inspired by M5 examples)
  if (thermal_initialized_ && thermal_max_sensor_ && thermal_min_sensor_ && thermal_max_sensor_->has_state() &&
      thermal_min_sensor_->has_state()) {
    float temp_range = thermal_max_sensor_->state - thermal_min_sensor_->state;

    // Color-code based on thermal activity (wider range = more thermal activity)
    uint16_t range_color = COLOR_TEXT;
    if (temp_range > 10.0) {
      range_color = COLOR_ALERT;  // High thermal activity
    } else if (temp_range > 5.0) {
      range_color = COLOR_WARNING;  // Moderate thermal activity
    } else {
      range_color = COLOR_GOOD;  // Low thermal activity
    }

    display_.setCursor(MARGIN, y);
    display_.setTextColor(COLOR_TEXT);
    display_.print("Range: ");
    display_.setTextColor(range_color);
    display_.print(temp_range, 1);
    display_.setTextColor(COLOR_TEXT);
    display_.print(" C  Med: ");
    display_.print(thermal_median_temp_, 1);
    display_.println(" C");
  }
}

void GrowEnvMonitor::draw_alerts_() {
  // Detailed alerts section on the left (where thermal data used to be)
  int y = MARGIN + 135;

  display_.setTextSize(1);
  display_.setTextColor(COLOR_HEADER);
  display_.setCursor(MARGIN, y);
  display_.println("Alerts");
  y += LINE_HEIGHT;

  bool has_alerts = false;

  // CO2 Alerts
  if (co2_sensor_ && co2_sensor_->has_state()) {
    float co2 = co2_sensor_->state;
    if (co2 > 1500) {
      display_.setCursor(MARGIN, y);
      display_.setTextColor(COLOR_ALERT);
      display_.println("CO2 Critical High");
      y += LINE_HEIGHT;
      has_alerts = true;
    } else if (co2 > 1000) {
      display_.setCursor(MARGIN, y);
      display_.setTextColor(COLOR_WARNING);
      display_.println("CO2 High");
      y += LINE_HEIGHT;
      has_alerts = true;
    } else if (co2 < 400) {
      display_.setCursor(MARGIN, y);
      display_.setTextColor(COLOR_WARNING);
      display_.println("CO2 Low");
      y += LINE_HEIGHT;
      has_alerts = true;
    }
  }

  // Temperature Alerts
  if (temperature_sensor_ && temperature_sensor_->has_state()) {
    float temp = temperature_sensor_->state;
    if (temp > 30) {
      display_.setCursor(MARGIN, y);
      display_.setTextColor(COLOR_ALERT);
      display_.println("Temperature High");
      y += LINE_HEIGHT;
      has_alerts = true;
    } else if (temp < 18) {
      display_.setCursor(MARGIN, y);
      display_.setTextColor(COLOR_ALERT);
      display_.println("Temperature Low");
      y += LINE_HEIGHT;
      has_alerts = true;
    } else if (temp > 28) {
      display_.setCursor(MARGIN, y);
      display_.setTextColor(COLOR_WARNING);
      display_.println("Temperature Warm");
      y += LINE_HEIGHT;
      has_alerts = true;
    } else if (temp < 20) {
      display_.setCursor(MARGIN, y);
      display_.setTextColor(COLOR_WARNING);
      display_.println("Temperature Cool");
      y += LINE_HEIGHT;
      has_alerts = true;
    }
  }

  // Humidity Alerts
  if (humidity_sensor_ && humidity_sensor_->has_state()) {
    float humidity = humidity_sensor_->state;
    if (humidity > 70) {
      display_.setCursor(MARGIN, y);
      display_.setTextColor(COLOR_ALERT);
      display_.println("Humidity High");
      y += LINE_HEIGHT;
      has_alerts = true;
    } else if (humidity < 40) {
      display_.setCursor(MARGIN, y);
      display_.setTextColor(COLOR_ALERT);
      display_.println("Humidity Low");
      y += LINE_HEIGHT;
      has_alerts = true;
    } else if (humidity > 60) {
      display_.setCursor(MARGIN, y);
      display_.setTextColor(COLOR_WARNING);
      display_.println("Humidity Elevated");
      y += LINE_HEIGHT;
      has_alerts = true;
    } else if (humidity < 50) {
      display_.setCursor(MARGIN, y);
      display_.setTextColor(COLOR_WARNING);
      display_.println("Humidity Dry");
      y += LINE_HEIGHT;
      has_alerts = true;
    }
  }

  // If no alerts, show "All Good"
  if (!has_alerts) {
    display_.setCursor(MARGIN, y);
    display_.setTextColor(COLOR_GOOD);
    display_.println("All Good");
  }
}

float GrowEnvMonitor::calculate_vpd_(float temperature, float humidity) {
  // Calculate VPD using standard formula
  float svp = 0.6108 * exp((17.27 * temperature) / (temperature + 237.3));
  float vpd = svp * (1 - humidity / 100);
  return vpd;
}

uint16_t GrowEnvMonitor::get_status_color_(float co2, float temp, float humidity) {
  // Check for alert conditions
  if (co2 > 1500 || temp > 30 || temp < 18 || humidity > 70 || humidity < 40) {
    return COLOR_ALERT;
  }

  // Check for warning conditions
  if (co2 > 1000 || temp > 28 || temp < 20 || humidity > 65 || humidity < 45) {
    return COLOR_WARNING;
  }

  return COLOR_GOOD;
}

void GrowEnvMonitor::setup_thermal_() {
  ESP_LOGCONFIG(TAG, "Setting up MLX90640 thermal camera...");

  // Initialize MLX90640 I2C driver (ESPHome handles I2C bus setup)
  MLX90640_I2CInit();
  MLX90640_I2CFreqSet(400);  // 400kHz for stable operation

  // Initialize MLX90640 parameters
  uint16_t eeMLX90640[832];
  int status = MLX90640_DumpEE(MLX90640_ADDRESS, eeMLX90640);
  if (status != 0) {
    ESP_LOGE(TAG, "Failed to load MLX90640 system parameters: %d", status);
    thermal_initialized_ = false;
    return;
  }

  status = MLX90640_ExtractParameters(eeMLX90640, &mlx90640_params_);
  if (status != 0) {
    ESP_LOGE(TAG, "MLX90640 parameter extraction failed: %d", status);
    thermal_initialized_ = false;
    return;
  }

  // Configure refresh rate from user setting
  uint16_t rate_code = parse_refresh_rate_(thermal_refresh_rate_);
  status = MLX90640_SetRefreshRate(MLX90640_ADDRESS, rate_code);
  if (status != 0) {
    ESP_LOGW(TAG, "Failed to set MLX90640 refresh rate: %d", status);
  } else {
    ESP_LOGCONFIG(TAG, "MLX90640 refresh rate set to %s", thermal_refresh_rate_.c_str());
  }

  // Use default thermal update interval (can be overridden by user configuration)
  ESP_LOGCONFIG(TAG, "Thermal update interval: %dms", thermal_update_interval_);

  // Configure resolution from user setting
  int resolution_bits = parse_resolution_(thermal_resolution_);
  status = setup_thermal_resolution_(resolution_bits);
  if (status != 0) {
    ESP_LOGW(TAG, "Failed to set MLX90640 resolution: %d", status);
  } else {
    ESP_LOGCONFIG(TAG, "MLX90640 resolution set to %s", thermal_resolution_.c_str());
  }

  // Configure pattern mode from user setting
  status = setup_thermal_pattern_(thermal_pattern_);
  if (status != 0) {
    ESP_LOGW(TAG, "Failed to set MLX90640 pattern mode: %d", status);
  } else {
    ESP_LOGCONFIG(TAG, "MLX90640 pattern mode set to %s", thermal_pattern_.c_str());
  }

  // Set active color palette
  set_active_palette_();

  thermal_initialized_ = true;
  ESP_LOGCONFIG(TAG, "MLX90640 thermal camera initialized successfully");
}

void GrowEnvMonitor::update_thermal_data_() {
  if (!thermal_initialized_)
    return;

  ESP_LOGD(TAG, "Reading MLX90640 thermal data...");

  // Read frames based on configuration (single frame for motion handling)
  bool frame_read = false;
  int max_frames = thermal_single_frame_ ? 1 : 2;
  int consecutive_failures = 0;

  for (byte x = 0; x < max_frames; x++) {  // Read single or both subpages
    uint16_t mlx90640Frame[834];
    uint32_t start_time = millis();
    int status = MLX90640_GetFrameData(MLX90640_ADDRESS, mlx90640Frame);
    uint32_t read_time = millis() - start_time;

    if (status < 0) {
      ESP_LOGD(TAG, "MLX90640 GetFrame subpage %d error: %d (took %dms)", x, status, read_time);
      consecutive_failures++;
      if (consecutive_failures > 3) {
        ESP_LOGW(TAG, "MLX90640 consecutive failures, skipping thermal update");
        return;
      }
      continue;
    }

    ESP_LOGD(TAG, "MLX90640 frame read successfully on subpage %d (took %dms)", x, read_time);

    // Calculate temperatures using this frame
    float vdd = MLX90640_GetVdd(mlx90640Frame, &mlx90640_params_);
    float Ta = MLX90640_GetTa(mlx90640Frame, &mlx90640_params_);
    float tr = Ta - TA_SHIFT;
    float emissivity = 0.95;

    // Calculate pixel temperatures
    MLX90640_CalculateTo(mlx90640Frame, &mlx90640_params_, emissivity, tr, mlx90640_pixels_);

    // Fix bad pixels using neighboring pixel interpolation
    MLX90640_BadPixelsCorrection(mlx90640_params_.brokenPixels, mlx90640_pixels_, 1, &mlx90640_params_);
    MLX90640_BadPixelsCorrection(mlx90640_params_.outlierPixels, mlx90640_pixels_, 1, &mlx90640_params_);

    // Interpolate thermal data from 24x32 to 48x64 for smoother display
    interpolate_image_(mlx90640_pixels_, 24, 32, interpolated_pixels_, 48, 64);

    frame_read = true;
    break;  // Successfully read one frame
  }

  if (!frame_read) {
    ESP_LOGD(TAG, "Failed to read any MLX90640 frames");
    return;
  }

  // Reset failure count on successful read
  consecutive_failures = 0;

  // Process thermal data similar to M5 examples with better statistics
  float min_temp = mlx90640_pixels_[0];
  float max_temp = mlx90640_pixels_[0];
  float sum_temp = 0;
  float valid_pixels[768];
  int valid_count = 0;

  // Filter out invalid/extreme readings and collect valid data
  for (int i = 0; i < 768; i++) {
    float temp = mlx90640_pixels_[i];
    // Filter out obviously bad readings (typical range -40°C to 85°C for MLX90640)
    if (temp > -40.0 && temp < 85.0) {
      valid_pixels[valid_count++] = temp;
      if (temp < min_temp)
        min_temp = temp;
      if (temp > max_temp)
        max_temp = temp;
      sum_temp += temp;
    }
  }

  if (valid_count > 0) {
    thermal_min_temp_ = min_temp;
    thermal_max_temp_ = max_temp;
    thermal_avg_temp_ = sum_temp / valid_count;

    // Calculate median temperature (like M5 examples)
    // Sort valid pixels for median calculation
    for (int i = 0; i < valid_count - 1; i++) {
      for (int j = i + 1; j < valid_count; j++) {
        if (valid_pixels[i] > valid_pixels[j]) {
          float temp = valid_pixels[i];
          valid_pixels[i] = valid_pixels[j];
          valid_pixels[j] = temp;
        }
      }
    }
    thermal_median_temp_ = valid_pixels[valid_count / 2];
  } else {
    ESP_LOGW(TAG, "No valid thermal readings - all pixels out of range");
    return;
  }

  // Update the template sensors only if values changed significantly
  if (thermal_min_sensor_ && (isnan(prev_thermal_min_) || fabs(thermal_min_temp_ - prev_thermal_min_) > 0.1)) {
    thermal_min_sensor_->publish_state(thermal_min_temp_);
    prev_thermal_min_ = thermal_min_temp_;
  }
  if (thermal_max_sensor_ && (isnan(prev_thermal_max_) || fabs(thermal_max_temp_ - prev_thermal_max_) > 0.1)) {
    thermal_max_sensor_->publish_state(thermal_max_temp_);
    prev_thermal_max_ = thermal_max_temp_;
  }
  if (thermal_avg_sensor_ && (isnan(prev_thermal_avg_) || fabs(thermal_avg_temp_ - prev_thermal_avg_) > 0.1)) {
    thermal_avg_sensor_->publish_state(thermal_avg_temp_);
    prev_thermal_avg_ = thermal_avg_temp_;
  }

  ESP_LOGD(TAG, "MLX90640 data (%d valid pixels) - Min: %.1f°C, Max: %.1f°C, Avg: %.1f°C, Median: %.1f°C", valid_count,
           thermal_min_temp_, thermal_max_temp_, thermal_avg_temp_, thermal_median_temp_);
}

// M5 Rainbow thermal color palette (256 colors)
const uint16_t GrowEnvMonitor::thermal_palette_rainbow_[256] = {
    0x0009, 0x0009, 0x0009, 0x0009, 0x0009, 0x0009, 0x0009, 0x0009, 0x000A, 0x002A, 0x002B, 0x004B, 0x006B, 0x008C,
    0x00AC, 0x00CC, 0x00ED, 0x010D, 0x010E, 0x012E, 0x014E, 0x014F, 0x0170, 0x0190, 0x0190, 0x0191, 0x01B1, 0x01B1,
    0x01B1, 0x01B2, 0x01D2, 0x01D2, 0x01F3, 0x01F3, 0x0213, 0x0214, 0x0234, 0x0234, 0x0235, 0x0255, 0x0256, 0x0276,
    0x0277, 0x0277, 0x0297, 0x0297, 0x02B8, 0x02B8, 0x02D9, 0x02D9, 0x02F9, 0x02F9, 0x02FA, 0x02FA, 0x031A, 0x031A,
    0x031A, 0x033B, 0x033B, 0x035B, 0x035B, 0x035B, 0x037B, 0x037B, 0x039B, 0x039B, 0x03BB, 0x03BB, 0x03DB, 0x03DB,
    0x03FB, 0x03FA, 0x041A, 0x0419, 0x0439, 0x0439, 0x0438, 0x0458, 0x0457, 0x0476, 0x0C75, 0x0C94, 0x0C94, 0x0C93,
    0x0C93, 0x0C92, 0x0CB1, 0x14B0, 0x14CF, 0x1CCE, 0x1CED, 0x24EC, 0x2D0B, 0x2D0A, 0x3529, 0x3D28, 0x4547, 0x4D66,
    0x4D66, 0x5585, 0x5D84, 0x5DA4, 0x65A3, 0x6DC3, 0x6DC2, 0x75E2, 0x75E2, 0x7DE2, 0x8601, 0x8601, 0x8E21, 0x8E21,
    0x9620, 0x9640, 0x9E40, 0xA640, 0xA660, 0xAE60, 0xAE60, 0xAE60, 0xB660, 0xBE80, 0xBE80, 0xC680, 0xC6A0, 0xC6A0,
    0xCEA0, 0xCEA0, 0xD6A0, 0xD6A0, 0xDEA0, 0xDEA0, 0xDEA0, 0xE6A0, 0xE6A0, 0xE6A0, 0xE680, 0xEE80, 0xEE80, 0xEE80,
    0xEE80, 0xEE80, 0xEE80, 0xEE81, 0xEE61, 0xF661, 0xF641, 0xF641, 0xF641, 0xF641, 0xF621, 0xF621, 0xF621, 0xFE01,
    0xFDE1, 0xFDE1, 0xFDC1, 0xFDC2, 0xFDA2, 0xFD82, 0xFD62, 0xFD42, 0xFD42, 0xFD22, 0xFD22, 0xFD02, 0xFD02, 0xFCE2,
    0xFCE2, 0xFCC3, 0xFCA3, 0xFC63, 0xFC43, 0xFC23, 0xFC03, 0xFBE4, 0xFBA4, 0xFB64, 0xFB44, 0xFB24, 0xFB04, 0xFAE5,
    0xFAC5, 0xFA85, 0xFA65, 0xFA25, 0xF9E6, 0xF9C6, 0xF9A6, 0xF986, 0xF966, 0xF927, 0xF907, 0xF907, 0xF8E7, 0xF8E7,
    0xF8E7, 0xF8E7, 0xF8C7, 0xF8C8, 0xF8C8, 0xF8C8, 0xF8C8, 0xF8C9, 0xF0C9, 0xF0C9, 0xF0C9, 0xF0CA, 0xF10A, 0xF10A,
    0xF12A, 0xF14B, 0xF16B, 0xF18B, 0xF9AB, 0xF9CC, 0xFA0C, 0xFA4C, 0xFA8D, 0xFAAD, 0xFAED, 0xFAED, 0xFB0D, 0xFB2D,
    0xFB2E, 0xFB2E, 0xFB6E, 0xFBAF, 0xFBCF, 0xFBEF, 0xFC10, 0xFC30, 0xFC50, 0xFC91, 0xFCB1, 0xFCF2, 0xFD12, 0xFD52,
    0xFD73, 0xFD93, 0xFD93, 0xFDD4, 0xFDF4, 0xFE15, 0xFE35, 0xFE55, 0xFE76, 0xFE96, 0xFED7, 0xFED7, 0xFEF8, 0xFEF9,
    0xFF19, 0xFF19, 0xFF39, 0xFF5A,
};

// M5 Golden thermal color palette (256 colors)
const uint16_t GrowEnvMonitor::thermal_palette_golden_[256] = {
    0x0004, 0x0004, 0x0004, 0x0004, 0x0005, 0x0005, 0x0825, 0x0825, 0x0825, 0x0826, 0x0826, 0x0826, 0x1027, 0x1027,
    0x1027, 0x1027, 0x1828, 0x1828, 0x1848, 0x1849, 0x2049, 0x2049, 0x204A, 0x204A, 0x284A, 0x284B, 0x284B, 0x284B,
    0x306C, 0x306C, 0x306C, 0x386D, 0x386D, 0x386D, 0x408E, 0x408E, 0x408E, 0x408F, 0x488F, 0x488F, 0x4890, 0x5090,
    0x50B0, 0x50B0, 0x58B1, 0x58B1, 0x58B1, 0x58B1, 0x60D2, 0x60D2, 0x60D2, 0x68D2, 0x68D2, 0x68D2, 0x68F3, 0x70F3,
    0x70F3, 0x70F3, 0x78F3, 0x7913, 0x7913, 0x7913, 0x8113, 0x8133, 0x8133, 0x8133, 0x8933, 0x8932, 0x8952, 0x9152,
    0x9152, 0x9152, 0x9151, 0x9971, 0x9971, 0x9971, 0x9970, 0xA190, 0xA190, 0xA18F, 0xA98F, 0xA9AF, 0xA9AE, 0xA9AE,
    0xB1AD, 0xB1CD, 0xB1CD, 0xB9CC, 0xB9EC, 0xB9EB, 0xB9EB, 0xC1EB, 0xC20A, 0xC20A, 0xCA09, 0xCA29, 0xCA29, 0xCA28,
    0xCA28, 0xD247, 0xD247, 0xD247, 0xDA66, 0xDA66, 0xDA65, 0xDA85, 0xDA85, 0xE284, 0xE2A4, 0xE2A4, 0xE2A3, 0xEAC3,
    0xEAC3, 0xEAE2, 0xEAE2, 0xEAE2, 0xF2E2, 0xF301, 0xF301, 0xF321, 0xF321, 0xF321, 0xF340, 0xFB40, 0xFB40, 0xFB60,
    0xFB60, 0xFB80, 0xFB80, 0xFB80, 0xFBA0, 0xFBA0, 0xFBC0, 0xFBC0, 0xFBE0, 0xFBE0, 0xFBE0, 0xFC00, 0xFC00, 0xFC20,
    0xFC20, 0xFC40, 0xFC40, 0xFC60, 0xFC60, 0xFC80, 0xFC80, 0xFC80, 0xFCA0, 0xFCA0, 0xFCC0, 0xFCE0, 0xFCE0, 0xFD00,
    0xFD00, 0xFD20, 0xFD20, 0xFD40, 0xFD40, 0xFD40, 0xFD60, 0xFD60, 0xFD80, 0xFDA0, 0xFDA0, 0xFDC0, 0xFDC0, 0xFDC1,
    0xFDE1, 0xFDE1, 0xFE01, 0xFE01, 0xFE21, 0xFE21, 0xFE41, 0xFE42, 0xFE62, 0xFE62, 0xFE62, 0xFE82, 0xFE82, 0xFE83,
    0xFEA3, 0xFEA3, 0xFEC3, 0xFEC3, 0xFEC3, 0xFEE3, 0xFEE4, 0xFEE4, 0xFF04, 0xFF04, 0xFF04, 0xFF25, 0xFF25, 0xFF25,
    0xFF45, 0xFF46, 0xFF46, 0xFF46, 0xFF67, 0xFF67, 0xFF67, 0xFF68, 0xFF68, 0xFF89, 0xFF89, 0xFF89, 0xFF8A, 0xFF8A,
    0xFFAB, 0xFFAB, 0xFFAC, 0xFFAC, 0xFFAD, 0xFFAD, 0xFFAE, 0xFFCE, 0xFFCE, 0xFFCF, 0xFFD0, 0xFFD0, 0xFFD1, 0xFFD1,
    0xFFD2, 0xFFD2, 0xFFD3, 0xFFD3, 0xFFD4, 0xFFD4, 0xFFD5, 0xFFF5, 0xFFF6, 0xFFF6, 0xFFF7, 0xFFF7, 0xFFF8, 0xFFF8,
    0xFFF9, 0xFFF9, 0xFFF9, 0xFFFA, 0xFFFA, 0xFFFB, 0xFFFB, 0xFFFC, 0xFFFC, 0xFFFC, 0xFFFD, 0xFFFD, 0xFFFD, 0xFFFE,
    0xFFFE, 0xFFFE, 0xFFFF, 0xFFFF,
};

// M5 Grayscale thermal color palette (256 colors)
const uint16_t GrowEnvMonitor::thermal_palette_grayscale_[256] = {
    0x0000, 0x0000, 0x0000, 0x0000, 0x0020, 0x0020, 0x0820, 0x0820, 0x0840, 0x0840, 0x0841, 0x0841, 0x0861, 0x0861,
    0x1061, 0x1061, 0x1081, 0x1081, 0x1082, 0x1082, 0x10a2, 0x10a2, 0x18a2, 0x18a2, 0x18c2, 0x18c2, 0x18c3, 0x18c3,
    0x18e3, 0x18e3, 0x20e3, 0x20e3, 0x2103, 0x2103, 0x2104, 0x2104, 0x2124, 0x2124, 0x2924, 0x2924, 0x2944, 0x2944,
    0x2945, 0x2945, 0x2965, 0x2965, 0x3165, 0x3165, 0x3185, 0x3185, 0x3186, 0x3186, 0x31a6, 0x31a6, 0x39a6, 0x39a6,
    0x39c6, 0x39c6, 0x39c7, 0x39c7, 0x39e7, 0x39e7, 0x41e7, 0x41e7, 0x4207, 0x4207, 0x4208, 0x4208, 0x4228, 0x4228,
    0x4a28, 0x4a28, 0x4a48, 0x4a48, 0x4a49, 0x4a49, 0x4a69, 0x4a69, 0x5269, 0x5269, 0x5289, 0x5289, 0x528a, 0x528a,
    0x52aa, 0x52aa, 0x5aaa, 0x5aaa, 0x5aca, 0x5aca, 0x5acb, 0x5acb, 0x5aeb, 0x5aeb, 0x62eb, 0x62eb, 0x630b, 0x630b,
    0x630c, 0x630c, 0x632c, 0x632c, 0x6b2c, 0x6b2c, 0x6b4c, 0x6b4c, 0x6b4d, 0x6b4d, 0x6b6d, 0x6b6d, 0x736d, 0x736d,
    0x738d, 0x738d, 0x738e, 0x738e, 0x73ae, 0x73ae, 0x7bae, 0x7bae, 0x7bce, 0x7bce, 0x7bcf, 0x7bcf, 0x7bef, 0x7bef,
    0x83ef, 0x83ef, 0x840f, 0x840f, 0x8410, 0x8410, 0x8430, 0x8430, 0x8c30, 0x8c30, 0x8c50, 0x8c50, 0x8c51, 0x8c51,
    0x8c71, 0x8c71, 0x9471, 0x9471, 0x9491, 0x9491, 0x9492, 0x9492, 0x94b2, 0x94b2, 0x9cb2, 0x9cb2, 0x9cd2, 0x9cd2,
    0x9cd3, 0x9cd3, 0x9cf3, 0x9cf3, 0xa4f3, 0xa4f3, 0xa513, 0xa513, 0xa514, 0xa514, 0xa534, 0xa534, 0xad34, 0xad34,
    0xad54, 0xad54, 0xad55, 0xad55, 0xad75, 0xad75, 0xb575, 0xb575, 0xb595, 0xb595, 0xb596, 0xb596, 0xb5b6, 0xb5b6,
    0xbdb6, 0xbdb6, 0xbdd6, 0xbdd6, 0xbdd7, 0xbdd7, 0xbdf7, 0xbdf7, 0xc5f7, 0xc5f7, 0xc617, 0xc617, 0xc618, 0xc618,
    0xc638, 0xc638, 0xce38, 0xce38, 0xce58, 0xce58, 0xce59, 0xce59, 0xce79, 0xce79, 0xd679, 0xd679, 0xd699, 0xd699,
    0xd69a, 0xd69a, 0xd6ba, 0xd6ba, 0xdeba, 0xdeba, 0xdeda, 0xdeda, 0xdedb, 0xdedb, 0xdefb, 0xdefb, 0xe6fb, 0xe6fb,
    0xe71b, 0xe71b, 0xe71c, 0xe71c, 0xe73c, 0xe73c, 0xef3c, 0xef3c, 0xef5c, 0xef5c, 0xef5d, 0xef5d, 0xef7d, 0xef7d,
    0xf77d, 0xf77d, 0xf79d, 0xf79d, 0xf79e, 0xf79e, 0xf7be, 0xf7be, 0xffbe, 0xffbe, 0xffde, 0xffde, 0xffdf, 0xffdf,
    0xffff, 0xffff, 0xffff, 0xffff,
};

// M5 Iron Black thermal color palette (256 colors)
const uint16_t GrowEnvMonitor::thermal_palette_ironblack_[256] = {
    0xFFFF, 0xFFFF, 0xFFDF, 0xFFDF, 0xF7BE, 0xF7BE, 0xF79E, 0xF79E, 0xEF7D, 0xEF7D, 0xEF5D, 0xEF5D, 0xE73C, 0xE73C,
    0xE71C, 0xE71C, 0xDEFB, 0xDEFB, 0xDEDB, 0xDEDB, 0xD6BA, 0xD6BA, 0xD69A, 0xD69A, 0xCE79, 0xCE79, 0xCE59, 0xCE59,
    0xC638, 0xC638, 0xC618, 0xC618, 0xBDF7, 0xBDF7, 0xBDD7, 0xBDD7, 0xB5B6, 0xB5B6, 0xB596, 0xB596, 0xAD75, 0xAD75,
    0xAD55, 0xAD55, 0xA534, 0xA534, 0xA514, 0xA514, 0x9CF3, 0x9CF3, 0x9CD3, 0x9CD3, 0x94B2, 0x94B2, 0x9492, 0x9492,
    0x8C71, 0x8C71, 0x8C51, 0x8C51, 0x8430, 0x8430, 0x8410, 0x8410, 0x7BEF, 0x7BEF, 0x7BCF, 0x7BCF, 0x73AE, 0x73AE,
    0x738E, 0x738E, 0x6B6D, 0x6B6D, 0x6B4D, 0x6B4D, 0x632C, 0x632C, 0x630C, 0x630C, 0x5AEB, 0x5AEB, 0x5ACB, 0x5ACB,
    0x52AA, 0x52AA, 0x528A, 0x528A, 0x4A69, 0x4A69, 0x4A49, 0x4A49, 0x4228, 0x4228, 0x4208, 0x4208, 0x39E7, 0x39E7,
    0x39C7, 0x39C7, 0x31A6, 0x31A6, 0x3186, 0x3186, 0x2965, 0x2965, 0x2945, 0x2945, 0x2124, 0x2124, 0x2104, 0x2104,
    0x18E3, 0x18E3, 0x18C3, 0x18C3, 0x10A2, 0x10A2, 0x1082, 0x1082, 0x0861, 0x0861, 0x0841, 0x0841, 0x0020, 0x0020,
    0x0000, 0x0000, 0x0001, 0x0002, 0x0003, 0x0003, 0x0804, 0x0805, 0x0806, 0x0807, 0x1008, 0x1009, 0x100A, 0x100B,
    0x180C, 0x180C, 0x180D, 0x180E, 0x200F, 0x280F, 0x280F, 0x300F, 0x380F, 0x380F, 0x400F, 0x400F, 0x4810, 0x5010,
    0x5010, 0x5810, 0x6010, 0x6010, 0x6810, 0x6810, 0x7011, 0x7811, 0x7811, 0x8011, 0x8011, 0x8811, 0x9011, 0x9031,
    0x9831, 0x9831, 0xA031, 0xA031, 0xA831, 0xB031, 0xB031, 0xB831, 0xB851, 0xB870, 0xC08F, 0xC08F, 0xC0AE, 0xC8CD,
    0xC8ED, 0xC8EC, 0xC90B, 0xD12B, 0xD14A, 0xD14A, 0xD969, 0xD988, 0xD9A8, 0xD9A7, 0xE1C6, 0xE1E5, 0xE205, 0xE205,
    0xE224, 0xE244, 0xE264, 0xE284, 0xE2A3, 0xEAC3, 0xEAE3, 0xEAE2, 0xEB02, 0xEB22, 0xEB41, 0xEB61, 0xEB81, 0xF3A1,
    0xF3A1, 0xF3C1, 0xF3E1, 0xF401, 0xF421, 0xF441, 0xF461, 0xF481, 0xF4A1, 0xF4C1, 0xF4E1, 0xF501, 0xF501, 0xF521,
    0xF541, 0xFD61, 0xFD81, 0xFDA2, 0xFDC2, 0xFDE2, 0xFE02, 0xFE22, 0xFE22, 0xFE42, 0xFE63, 0xFE83, 0xFEA3, 0xFEC3,
    0xFEE3, 0xFF03, 0xFF04, 0xFF26, 0xFF28, 0xFF4A, 0xFF4B, 0xFF6D, 0xFF6F, 0xFF91, 0xFF92, 0xFFB4, 0xFFB6, 0xFFD8,
    0xFFD9, 0xFFDB, 0xFFFD, 0xFFE3,
};

// M5 CAM thermal color palette (256 colors)
const uint16_t GrowEnvMonitor::thermal_palette_cam_[256] = {
    0x480F, 0x400F, 0x400F, 0x400F, 0x4010, 0x3810, 0x3810, 0x3810, 0x3810, 0x3010, 0x3010, 0x3010, 0x2810, 0x2810,
    0x2810, 0x2810, 0x2010, 0x2010, 0x2010, 0x1810, 0x1810, 0x1811, 0x1811, 0x1011, 0x1011, 0x1011, 0x0811, 0x0811,
    0x0811, 0x0011, 0x0011, 0x0011, 0x0011, 0x0011, 0x0031, 0x0031, 0x0051, 0x0072, 0x0072, 0x0092, 0x00B2, 0x00B2,
    0x00D2, 0x00F2, 0x00F2, 0x0112, 0x0132, 0x0152, 0x0152, 0x0172, 0x0192, 0x0192, 0x01B2, 0x01D2, 0x01F3, 0x01F3,
    0x0213, 0x0233, 0x0253, 0x0253, 0x0273, 0x0293, 0x02B3, 0x02D3, 0x02D3, 0x02F3, 0x0313, 0x0333, 0x0333, 0x0353,
    0x0373, 0x0394, 0x03B4, 0x03D4, 0x03D4, 0x03F4, 0x0414, 0x0434, 0x0454, 0x0474, 0x0474, 0x0494, 0x04B4, 0x04D4,
    0x04F4, 0x0514, 0x0534, 0x0534, 0x0554, 0x0554, 0x0574, 0x0574, 0x0573, 0x0573, 0x0573, 0x0572, 0x0572, 0x0572,
    0x0571, 0x0591, 0x0591, 0x0590, 0x0590, 0x058F, 0x058F, 0x058F, 0x058E, 0x05AE, 0x05AE, 0x05AD, 0x05AD, 0x05AD,
    0x05AC, 0x05AC, 0x05AB, 0x05CB, 0x05CB, 0x05CA, 0x05CA, 0x05CA, 0x05C9, 0x05C9, 0x05C8, 0x05E8, 0x05E8, 0x05E7,
    0x05E7, 0x05E6, 0x05E6, 0x05E6, 0x05E5, 0x05E5, 0x0604, 0x0604, 0x0604, 0x0603, 0x0603, 0x0602, 0x0602, 0x0601,
    0x0621, 0x0621, 0x0620, 0x0620, 0x0620, 0x0620, 0x0E20, 0x0E20, 0x0E40, 0x1640, 0x1640, 0x1E40, 0x1E40, 0x2640,
    0x2640, 0x2E40, 0x2E60, 0x3660, 0x3660, 0x3E60, 0x3E60, 0x3E60, 0x4660, 0x4660, 0x4E60, 0x4E80, 0x5680, 0x5680,
    0x5E80, 0x5E80, 0x6680, 0x6680, 0x6E80, 0x6EA0, 0x76A0, 0x76A0, 0x7EA0, 0x7EA0, 0x86A0, 0x86A0, 0x8EA0, 0x8EC0,
    0x96C0, 0x96C0, 0x9EC0, 0x9EC0, 0xA6C0, 0xAEC0, 0xAEC0, 0xB6E0, 0xB6E0, 0xBEE0, 0xBEE0, 0xC6E0, 0xC6E0, 0xCEE0,
    0xCEE0, 0xD6E0, 0xD700, 0xDF00, 0xDEE0, 0xDEC0, 0xDEA0, 0xDE80, 0xDE80, 0xE660, 0xE640, 0xE620, 0xE600, 0xE5E0,
    0xE5C0, 0xE5A0, 0xE580, 0xE560, 0xE540, 0xE520, 0xE500, 0xE4E0, 0xE4C0, 0xE4A0, 0xE480, 0xE460, 0xEC40, 0xEC20,
    0xEC00, 0xEBE0, 0xEBC0, 0xEBA0, 0xEB80, 0xEB60, 0xEB40, 0xEB20, 0xEB00, 0xEAE0, 0xEAC0, 0xEAA0, 0xEA80, 0xEA60,
    0xEA40, 0xF220, 0xF200, 0xF1E0, 0xF1C0, 0xF1A0, 0xF180, 0xF160, 0xF140, 0xF100, 0xF0E0, 0xF0C0, 0xF0A0, 0xF080,
    0xF060, 0xF040, 0xF020, 0xF800,
};

// FLIR-style thermal palettes based on professional thermal imaging standards
// Ironbow palette - black through blue, magenta, orange, yellow to white
const uint16_t GrowEnvMonitor::thermal_palette_ironbow_[256] = {
    0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
    0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0001, 0x0001, 0x0001, 0x0001, 0x0001, 0x0001, 0x0001, 0x0001, 0x0001,
    0x0001, 0x0001, 0x0001, 0x0001, 0x0001, 0x0001, 0x0001, 0x0002, 0x0002, 0x0002, 0x0002, 0x0002, 0x0002, 0x0002,
    0x0002, 0x0002, 0x0002, 0x0002, 0x0002, 0x0002, 0x0002, 0x0002, 0x0002, 0x0003, 0x0003, 0x0003, 0x0003, 0x0003,
    0x0003, 0x0003, 0x0003, 0x0003, 0x0003, 0x0003, 0x0003, 0x0003, 0x0003, 0x0003, 0x0003, 0x0004, 0x0004, 0x0004,
    0x0004, 0x0004, 0x0004, 0x0004, 0x0004, 0x0004, 0x0004, 0x0004, 0x0004, 0x0004, 0x0004, 0x0004, 0x0004, 0x0005,
    0x0005, 0x0005, 0x0005, 0x0005, 0x0005, 0x0005, 0x0005, 0x0005, 0x0005, 0x0005, 0x0005, 0x0005, 0x0005, 0x0005,
    0x0005, 0x0006, 0x0006, 0x0006, 0x0006, 0x0006, 0x0006, 0x0006, 0x0006, 0x0006, 0x0006, 0x0006, 0x0006, 0x0006,
    0x0006, 0x0006, 0x0006, 0x0007, 0x0007, 0x0007, 0x0007, 0x0007, 0x0007, 0x0007, 0x0007, 0x0007, 0x0007, 0x0007,
    0x0007, 0x0007, 0x0007, 0x0007, 0x0007, 0x0008, 0x0008, 0x0008, 0x0008, 0x0008, 0x0008, 0x0008, 0x0008, 0x0008,
    0x0008, 0x0008, 0x0008, 0x0008, 0x0008, 0x0008, 0x0008, 0x0009, 0x0009, 0x0009, 0x0009, 0x0009, 0x0009, 0x0009,
    0x0009, 0x0009, 0x0009, 0x0009, 0x0009, 0x0009, 0x0009, 0x0009, 0x0009, 0x000A, 0x000A, 0x000A, 0x000A, 0x000A,
    0x000A, 0x000A, 0x000A, 0x000A, 0x000A, 0x000A, 0x000A, 0x000A, 0x000A, 0x000A, 0x000A, 0x000B, 0x000B, 0x000B,
    0x000B, 0x000B, 0x000B, 0x000B, 0x000B, 0x000B, 0x000B, 0x000B, 0x000B, 0x000B, 0x000B, 0x000B, 0x000B, 0x000C,
    0x000C, 0x000C, 0x000C, 0x000C, 0x000C, 0x000C, 0x000C, 0x000C, 0x000C, 0x000C, 0x000C, 0x000C, 0x000C, 0x000C,
    0x000C, 0x000D, 0x000D, 0x000D, 0x000D, 0x000D, 0x000D, 0x000D, 0x000D, 0x000D, 0x000D, 0x000D, 0x000D, 0x000D,
    0x000D, 0x000D, 0x000D, 0x000E, 0x000E, 0x000E, 0x000E, 0x000E, 0x000E, 0x000E, 0x000E, 0x000E, 0x000E, 0x000E,
    0x000E, 0x000E, 0x000E, 0x000E, 0x000E, 0x000F, 0x000F, 0x000F, 0x000F, 0x000F, 0x000F, 0x000F, 0x000F, 0x000F,
    0x000F, 0x000F, 0x000F, 0x000F,
};

// Arctic palette - blues for cold to golden yellows/red for warm
const uint16_t GrowEnvMonitor::thermal_palette_arctic_[256] = {
    0x001F, 0x001F, 0x001F, 0x001F, 0x001F, 0x001F, 0x001F, 0x001F, 0x001F, 0x001F, 0x001F, 0x001F, 0x001F, 0x001F,
    0x001F, 0x001F, 0x003F, 0x003F, 0x003F, 0x003F, 0x003F, 0x003F, 0x003F, 0x003F, 0x005F, 0x005F, 0x005F, 0x005F,
    0x005F, 0x005F, 0x005F, 0x005F, 0x007F, 0x007F, 0x007F, 0x007F, 0x007F, 0x007F, 0x007F, 0x007F, 0x009F, 0x009F,
    0x009F, 0x009F, 0x009F, 0x009F, 0x009F, 0x009F, 0x00BF, 0x00BF, 0x00BF, 0x00BF, 0x00BF, 0x00BF, 0x00BF, 0x00BF,
    0x00DF, 0x00DF, 0x00DF, 0x00DF, 0x00DF, 0x00DF, 0x00DF, 0x00DF, 0x00FF, 0x00FF, 0x00FF, 0x00FF, 0x00FF, 0x00FF,
    0x00FF, 0x00FF, 0x01FF, 0x01FF, 0x01FF, 0x01FF, 0x01FF, 0x01FF, 0x01FF, 0x01FF, 0x03FF, 0x03FF, 0x03FF, 0x03FF,
    0x03FF, 0x03FF, 0x03FF, 0x03FF, 0x05FF, 0x05FF, 0x05FF, 0x05FF, 0x05FF, 0x05FF, 0x05FF, 0x05FF, 0x07FF, 0x07FF,
    0x07FF, 0x07FF, 0x07FF, 0x07FF, 0x07FF, 0x07FF, 0x0FFF, 0x0FFF, 0x0FFF, 0x0FFF, 0x0FFF, 0x0FFF, 0x0FFF, 0x0FFF,
    0x1FFF, 0x1FFF, 0x1FFF, 0x1FFF, 0x1FFF, 0x1FFF, 0x1FFF, 0x1FFF, 0x3FFF, 0x3FFF, 0x3FFF, 0x3FFF, 0x3FFF, 0x3FFF,
    0x3FFF, 0x3FFF, 0x5FFF, 0x5FFF, 0x5FFF, 0x5FFF, 0x5FFF, 0x5FFF, 0x5FFF, 0x5FFF, 0x7FFF, 0x7FFF, 0x7FFF, 0x7FFF,
    0x7FFF, 0x7FFF, 0x7FFF, 0x7FFF, 0x7FE0, 0x7FE0, 0x7FE0, 0x7FE0, 0x7FE0, 0x7FE0, 0x7FE0, 0x7FE0, 0x7FC0, 0x7FC0,
    0x7FC0, 0x7FC0, 0x7FC0, 0x7FC0, 0x7FC0, 0x7FC0, 0x7FA0, 0x7FA0, 0x7FA0, 0x7FA0, 0x7FA0, 0x7FA0, 0x7FA0, 0x7FA0,
    0x7F80, 0x7F80, 0x7F80, 0x7F80, 0x7F80, 0x7F80, 0x7F80, 0x7F80, 0x7F60, 0x7F60, 0x7F60, 0x7F60, 0x7F60, 0x7F60,
    0x7F60, 0x7F60, 0x7F40, 0x7F40, 0x7F40, 0x7F40, 0x7F40, 0x7F40, 0x7F40, 0x7F40, 0x7F20, 0x7F20, 0x7F20, 0x7F20,
    0x7F20, 0x7F20, 0x7F20, 0x7F20, 0x7F00, 0x7F00, 0x7F00, 0x7F00, 0x7F00, 0x7F00, 0x7F00, 0x7F00, 0x7E00, 0x7E00,
    0x7E00, 0x7E00, 0x7E00, 0x7E00, 0x7E00, 0x7E00, 0x7C00, 0x7C00, 0x7C00, 0x7C00, 0x7C00, 0x7C00, 0x7C00, 0x7C00,
    0x7800, 0x7800, 0x7800, 0x7800, 0x7800, 0x7800, 0x7800, 0x7800, 0x7000, 0x7000, 0x7000, 0x7000, 0x7000, 0x7000,
    0x7000, 0x7000, 0x6000, 0x6000, 0x6000, 0x6000, 0x6000, 0x6000, 0x6000, 0x6000, 0x4000, 0x4000, 0x4000, 0x4000,
    0x4000, 0x4000, 0x4000, 0x4000,
};

// Lava palette - similar to ironbow, hot objects in warm colors, cold in blue
const uint16_t GrowEnvMonitor::thermal_palette_lava_[256] = {
    0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
    0x0000, 0x0000, 0x0800, 0x0800, 0x0800, 0x0800, 0x0800, 0x0800, 0x0800, 0x0800, 0x0800, 0x0800, 0x0800, 0x0800,
    0x0800, 0x0800, 0x0800, 0x0800, 0x1000, 0x1000, 0x1000, 0x1000, 0x1000, 0x1000, 0x1000, 0x1000, 0x1000, 0x1000,
    0x1000, 0x1000, 0x1000, 0x1000, 0x1000, 0x1000, 0x1800, 0x1800, 0x1800, 0x1800, 0x1800, 0x1800, 0x1800, 0x1800,
    0x1800, 0x1800, 0x1800, 0x1800, 0x1800, 0x1800, 0x1800, 0x1800, 0x2000, 0x2000, 0x2000, 0x2000, 0x2000, 0x2000,
    0x2000, 0x2000, 0x2000, 0x2000, 0x2000, 0x2000, 0x2000, 0x2000, 0x2000, 0x2000, 0x2800, 0x2800, 0x2800, 0x2800,
    0x2800, 0x2800, 0x2800, 0x2800, 0x2800, 0x2800, 0x2800, 0x2800, 0x2800, 0x2800, 0x2800, 0x2800, 0x3000, 0x3000,
    0x3000, 0x3000, 0x3000, 0x3000, 0x3000, 0x3000, 0x3000, 0x3000, 0x3000, 0x3000, 0x3000, 0x3000, 0x3000, 0x3000,
    0x3800, 0x3800, 0x3800, 0x3800, 0x3800, 0x3800, 0x3800, 0x3800, 0x3800, 0x3800, 0x3800, 0x3800, 0x3800, 0x3800,
    0x3800, 0x3800, 0x4000, 0x4000, 0x4000, 0x4000, 0x4000, 0x4000, 0x4000, 0x4000, 0x4000, 0x4000, 0x4000, 0x4000,
    0x4000, 0x4000, 0x4000, 0x4000, 0x4800, 0x4800, 0x4800, 0x4800, 0x4800, 0x4800, 0x4800, 0x4800, 0x4800, 0x4800,
    0x4800, 0x4800, 0x4800, 0x4800, 0x4800, 0x4800, 0x5000, 0x5000, 0x5000, 0x5000, 0x5000, 0x5000, 0x5000, 0x5000,
    0x5000, 0x5000, 0x5000, 0x5000, 0x5000, 0x5000, 0x5000, 0x5000, 0x5800, 0x5800, 0x5800, 0x5800, 0x5800, 0x5800,
    0x5800, 0x5800, 0x5800, 0x5800, 0x5800, 0x5800, 0x5800, 0x5800, 0x5800, 0x5800, 0x6000, 0x6000, 0x6000, 0x6000,
    0x6000, 0x6000, 0x6000, 0x6000, 0x6000, 0x6000, 0x6000, 0x6000, 0x6000, 0x6000, 0x6000, 0x6000, 0x6800, 0x6800,
    0x6800, 0x6800, 0x6800, 0x6800, 0x6800, 0x6800, 0x6800, 0x6800, 0x6800, 0x6800, 0x6800, 0x6800, 0x6800, 0x6800,
    0x7000, 0x7000, 0x7000, 0x7000, 0x7000, 0x7000, 0x7000, 0x7000, 0x7000, 0x7000, 0x7000, 0x7000, 0x7000, 0x7000,
    0x7000, 0x7000, 0x7800, 0x7800, 0x7800, 0x7800, 0x7800, 0x7800, 0x7800, 0x7800, 0x7800, 0x7800, 0x7800, 0x7800,
    0x7800, 0x7800, 0x7800, 0x7800,
};

// White hot palette - grayscale with hot=white, cold=black
const uint16_t GrowEnvMonitor::thermal_palette_whitehot_[256] = {
    0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0841, 0x0841, 0x0841, 0x0841, 0x0841, 0x0841,
    0x0841, 0x0841, 0x1082, 0x1082, 0x1082, 0x1082, 0x1082, 0x1082, 0x1082, 0x1082, 0x18C3, 0x18C3, 0x18C3, 0x18C3,
    0x18C3, 0x18C3, 0x18C3, 0x18C3, 0x2104, 0x2104, 0x2104, 0x2104, 0x2104, 0x2104, 0x2104, 0x2104, 0x2945, 0x2945,
    0x2945, 0x2945, 0x2945, 0x2945, 0x2945, 0x2945, 0x3186, 0x3186, 0x3186, 0x3186, 0x3186, 0x3186, 0x3186, 0x3186,
    0x39C7, 0x39C7, 0x39C7, 0x39C7, 0x39C7, 0x39C7, 0x39C7, 0x39C7, 0x4208, 0x4208, 0x4208, 0x4208, 0x4208, 0x4208,
    0x4208, 0x4208, 0x4A49, 0x4A49, 0x4A49, 0x4A49, 0x4A49, 0x4A49, 0x4A49, 0x4A49, 0x528A, 0x528A, 0x528A, 0x528A,
    0x528A, 0x528A, 0x528A, 0x528A, 0x5ACB, 0x5ACB, 0x5ACB, 0x5ACB, 0x5ACB, 0x5ACB, 0x5ACB, 0x5ACB, 0x630C, 0x630C,
    0x630C, 0x630C, 0x630C, 0x630C, 0x630C, 0x630C, 0x6B4D, 0x6B4D, 0x6B4D, 0x6B4D, 0x6B4D, 0x6B4D, 0x6B4D, 0x6B4D,
    0x738E, 0x738E, 0x738E, 0x738E, 0x738E, 0x738E, 0x738E, 0x738E, 0x7BCF, 0x7BCF, 0x7BCF, 0x7BCF, 0x7BCF, 0x7BCF,
    0x7BCF, 0x7BCF, 0x8410, 0x8410, 0x8410, 0x8410, 0x8410, 0x8410, 0x8410, 0x8410, 0x8C51, 0x8C51, 0x8C51, 0x8C51,
    0x8C51, 0x8C51, 0x8C51, 0x8C51, 0x9492, 0x9492, 0x9492, 0x9492, 0x9492, 0x9492, 0x9492, 0x9492, 0x9CD3, 0x9CD3,
    0x9CD3, 0x9CD3, 0x9CD3, 0x9CD3, 0x9CD3, 0x9CD3, 0xA514, 0xA514, 0xA514, 0xA514, 0xA514, 0xA514, 0xA514, 0xA514,
    0xAD55, 0xAD55, 0xAD55, 0xAD55, 0xAD55, 0xAD55, 0xAD55, 0xAD55, 0xB596, 0xB596, 0xB596, 0xB596, 0xB596, 0xB596,
    0xB596, 0xB596, 0xBDD7, 0xBDD7, 0xBDD7, 0xBDD7, 0xBDD7, 0xBDD7, 0xBDD7, 0xBDD7, 0xC618, 0xC618, 0xC618, 0xC618,
    0xC618, 0xC618, 0xC618, 0xC618, 0xCE59, 0xCE59, 0xCE59, 0xCE59, 0xCE59, 0xCE59, 0xCE59, 0xCE59, 0xD69A, 0xD69A,
    0xD69A, 0xD69A, 0xD69A, 0xD69A, 0xD69A, 0xD69A, 0xDEDB, 0xDEDB, 0xDEDB, 0xDEDB, 0xDEDB, 0xDEDB, 0xDEDB, 0xDEDB,
    0xE71C, 0xE71C, 0xE71C, 0xE71C, 0xE71C, 0xE71C, 0xE71C, 0xE71C, 0xEF5D, 0xEF5D, 0xEF5D, 0xEF5D, 0xEF5D, 0xEF5D,
    0xEF5D, 0xEF5D, 0xF79E, 0xF79E, 0xF79E, 0xF79E, 0xF79E, 0xF79E, 0xF79E, 0xF79E, 0xFFDF, 0xFFDF, 0xFFDF, 0xFFDF,
    0xFFDF, 0xFFDF, 0xFFDF, 0xFFDF,
};

// Black hot palette - inverted grayscale with hot=black, cold=white
const uint16_t GrowEnvMonitor::thermal_palette_blackhot_[256] = {
    0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFDF, 0xFFDF, 0xFFDF, 0xFFDF, 0xFFDF, 0xFFDF,
    0xFFDF, 0xFFDF, 0xF79E, 0xF79E, 0xF79E, 0xF79E, 0xF79E, 0xF79E, 0xF79E, 0xF79E, 0xEF5D, 0xEF5D, 0xEF5D, 0xEF5D,
    0xEF5D, 0xEF5D, 0xEF5D, 0xEF5D, 0xE71C, 0xE71C, 0xE71C, 0xE71C, 0xE71C, 0xE71C, 0xE71C, 0xE71C, 0xDEDB, 0xDEDB,
    0xDEDB, 0xDEDB, 0xDEDB, 0xDEDB, 0xDEDB, 0xDEDB, 0xD69A, 0xD69A, 0xD69A, 0xD69A, 0xD69A, 0xD69A, 0xD69A, 0xD69A,
    0xCE59, 0xCE59, 0xCE59, 0xCE59, 0xCE59, 0xCE59, 0xCE59, 0xCE59, 0xC618, 0xC618, 0xC618, 0xC618, 0xC618, 0xC618,
    0xC618, 0xC618, 0xBDD7, 0xBDD7, 0xBDD7, 0xBDD7, 0xBDD7, 0xBDD7, 0xBDD7, 0xBDD7, 0xB596, 0xB596, 0xB596, 0xB596,
    0xB596, 0xB596, 0xB596, 0xB596, 0xAD55, 0xAD55, 0xAD55, 0xAD55, 0xAD55, 0xAD55, 0xAD55, 0xAD55, 0xA514, 0xA514,
    0xA514, 0xA514, 0xA514, 0xA514, 0xA514, 0xA514, 0x9CD3, 0x9CD3, 0x9CD3, 0x9CD3, 0x9CD3, 0x9CD3, 0x9CD3, 0x9CD3,
    0x9492, 0x9492, 0x9492, 0x9492, 0x9492, 0x9492, 0x9492, 0x9492, 0x8C51, 0x8C51, 0x8C51, 0x8C51, 0x8C51, 0x8C51,
    0x8C51, 0x8C51, 0x8410, 0x8410, 0x8410, 0x8410, 0x8410, 0x8410, 0x8410, 0x8410, 0x7BCF, 0x7BCF, 0x7BCF, 0x7BCF,
    0x7BCF, 0x7BCF, 0x7BCF, 0x7BCF, 0x738E, 0x738E, 0x738E, 0x738E, 0x738E, 0x738E, 0x738E, 0x738E, 0x6B4D, 0x6B4D,
    0x6B4D, 0x6B4D, 0x6B4D, 0x6B4D, 0x6B4D, 0x6B4D, 0x630C, 0x630C, 0x630C, 0x630C, 0x630C, 0x630C, 0x630C, 0x630C,
    0x5ACB, 0x5ACB, 0x5ACB, 0x5ACB, 0x5ACB, 0x5ACB, 0x5ACB, 0x5ACB, 0x528A, 0x528A, 0x528A, 0x528A, 0x528A, 0x528A,
    0x528A, 0x528A, 0x4A49, 0x4A49, 0x4A49, 0x4A49, 0x4A49, 0x4A49, 0x4A49, 0x4A49, 0x4208, 0x4208, 0x4208, 0x4208,
    0x4208, 0x4208, 0x4208, 0x4208, 0x39C7, 0x39C7, 0x39C7, 0x39C7, 0x39C7, 0x39C7, 0x39C7, 0x39C7, 0x3186, 0x3186,
    0x3186, 0x3186, 0x3186, 0x3186, 0x3186, 0x3186, 0x2945, 0x2945, 0x2945, 0x2945, 0x2945, 0x2945, 0x2945, 0x2945,
    0x2104, 0x2104, 0x2104, 0x2104, 0x2104, 0x2104, 0x2104, 0x2104, 0x18C3, 0x18C3, 0x18C3, 0x18C3, 0x18C3, 0x18C3,
    0x18C3, 0x18C3, 0x1082, 0x1082, 0x1082, 0x1082, 0x1082, 0x1082, 0x1082, 0x1082, 0x0841, 0x0841, 0x0841, 0x0841,
    0x0841, 0x0841, 0x0841, 0x0841,
};

uint16_t GrowEnvMonitor::parse_refresh_rate_(const std::string &rate_str) {
  if (rate_str == "0.5Hz")
    return 0x00;
  if (rate_str == "1Hz")
    return 0x01;
  if (rate_str == "2Hz")
    return 0x02;
  if (rate_str == "4Hz")
    return 0x03;
  if (rate_str == "8Hz")
    return 0x04;
  if (rate_str == "16Hz")
    return 0x05;
  if (rate_str == "32Hz")
    return 0x06;
  if (rate_str == "64Hz")
    return 0x07;
  return 0x05;  // Default 16Hz
}

int GrowEnvMonitor::parse_resolution_(const std::string &res_str) {
  if (res_str == "16-bit")
    return 16;
  if (res_str == "17-bit")
    return 17;
  if (res_str == "18-bit")
    return 18;
  if (res_str == "19-bit")
    return 19;
  return 18;  // Default 18-bit
}

int GrowEnvMonitor::setup_thermal_resolution_(int bits) {
  uint16_t controlRegister1;
  int error = MLX90640_I2CRead(MLX90640_ADDRESS, 0x800D, 1, &controlRegister1);
  if (error != 0)
    return error;

  // Clear resolution bits (B11, B10)
  controlRegister1 &= 0xF3FF;

  // Set resolution bits
  switch (bits) {
    case 16: /* 0 0 already set */
      break;
    case 17:
      controlRegister1 |= 0x0400;
      break;  // Set B10
    case 18:
      controlRegister1 |= 0x0800;
      break;  // Set B11
    case 19:
      controlRegister1 |= 0x0C00;
      break;  // Set B11, B10
  }

  return MLX90640_I2CWrite(MLX90640_ADDRESS, 0x800D, controlRegister1);
}

int GrowEnvMonitor::setup_thermal_pattern_(const std::string &pattern) {
  if (pattern == "chess") {
    return MLX90640_SetChessMode(MLX90640_ADDRESS);
  } else if (pattern == "interleaved") {
    return MLX90640_SetInterleavedMode(MLX90640_ADDRESS);
  }
  return -1;  // Invalid pattern
}

void GrowEnvMonitor::set_active_palette_() {
  if (thermal_palette_ == "rainbow") {
    current_palette_ = thermal_palette_rainbow_;
  } else if (thermal_palette_ == "golden") {
    current_palette_ = thermal_palette_golden_;
  } else if (thermal_palette_ == "grayscale") {
    current_palette_ = thermal_palette_grayscale_;
  } else if (thermal_palette_ == "ironblack") {
    current_palette_ = thermal_palette_ironblack_;
  } else if (thermal_palette_ == "cam") {
    current_palette_ = thermal_palette_cam_;
  } else if (thermal_palette_ == "ironbow") {
    current_palette_ = thermal_palette_ironbow_;
  } else if (thermal_palette_ == "arctic") {
    current_palette_ = thermal_palette_arctic_;
  } else if (thermal_palette_ == "lava") {
    current_palette_ = thermal_palette_lava_;
  } else if (thermal_palette_ == "whitehot") {
    current_palette_ = thermal_palette_whitehot_;
  } else if (thermal_palette_ == "blackhot") {
    current_palette_ = thermal_palette_blackhot_;
  } else {
    current_palette_ = thermal_palette_rainbow_;  // Default fallback
  }

  ESP_LOGCONFIG(TAG, "MLX90640 color palette set to: %s", thermal_palette_.c_str());
}

uint16_t GrowEnvMonitor::temp_to_color_(float temperature, float min_temp, float max_temp) {
  // Normalize temperature to 0-255 range
  int color_index = 0;
  if (max_temp > min_temp) {
    float normalized = (temperature - min_temp) / (max_temp - min_temp);
    normalized = (normalized < 0.0f) ? 0.0f : (normalized > 1.0f) ? 1.0f : normalized;
    color_index = (int) (normalized * 255.0f);
  }
  color_index = (color_index < 0) ? 0 : (color_index > 255) ? 255 : color_index;

  // Use active palette instead of hardcoded rainbow
  return current_palette_ ? current_palette_[color_index] : thermal_palette_rainbow_[color_index];
}

void GrowEnvMonitor::generate_thermal_image_(uint8_t *rgb_buffer) {
  if (!thermal_initialized_ || !rgb_buffer)
    return;

  // Use valid thermal temperature range for color mapping
  float min_temp = thermal_min_temp_;
  float max_temp = thermal_max_temp_;

  // Ensure reasonable temperature range for color mapping
  if (max_temp - min_temp < 1.0f) {
    max_temp = min_temp + 10.0f;  // Default 10°C range
  }

  // Generate 64x48 RGB image from interpolated thermal data
  for (int y = 0; y < 48; y++) {
    for (int x = 0; x < 64; x++) {
      int pixel_idx = y * 64 + x;
      float temperature = interpolated_pixels_[pixel_idx];

      // Get 16-bit color from palette
      uint16_t color565 = temp_to_color_(temperature, min_temp, max_temp);

      // Convert RGB565 to RGB888
      uint8_t r = ((color565 >> 11) & 0x1F) << 3;  // 5 bits -> 8 bits
      uint8_t g = ((color565 >> 5) & 0x3F) << 2;   // 6 bits -> 8 bits
      uint8_t b = (color565 & 0x1F) << 3;          // 5 bits -> 8 bits

      // Store in RGB buffer
      int rgb_idx = pixel_idx * 3;
      rgb_buffer[rgb_idx] = r;
      rgb_buffer[rgb_idx + 1] = g;
      rgb_buffer[rgb_idx + 2] = b;
    }
  }
}

void GrowEnvMonitor::draw_thermal_image_() {
  if (!thermal_initialized_)
    return;

  // Position thermal image in right middle center area
  int image_x = SCREEN_WIDTH - 180;   // 180 pixels from right edge (good horizontal position)
  int image_y = SCREEN_HEIGHT - 170;  // 170 pixels from bottom (moved down slightly)
  int image_w = 160;                  // Double the width (was 80)
  int image_h = 120;                  // Double the height (was 60)

  // Ensure we don't go off screen
  if (image_x < 0)
    image_x = SCREEN_WIDTH - image_w - 5;
  if (image_y < 0)
    image_y = SCREEN_HEIGHT - image_h - 5;

  display_.setTextSize(1);
  display_.setTextColor(COLOR_HEADER);
  display_.setCursor(image_x, image_y - 15);
  display_.println("Thermal");

  // Draw thermal temperature info below the image
  int info_y = image_y + image_h + 5;

  // Clear the thermal temperature text area before drawing
  display_.fillRect(image_x, info_y, image_w, 24, COLOR_BACKGROUND);

  display_.setTextSize(1);
  display_.setTextColor(COLOR_TEXT);

  // Min temp
  display_.setCursor(image_x, info_y);
  display_.print("Min: ");
  if (thermal_min_sensor_ && thermal_min_sensor_->has_state()) {
    display_.print(thermal_min_sensor_->state, 1);
    display_.print("C");
  } else {
    display_.print("--");
  }

  // Max temp (same line, offset)
  display_.setCursor(image_x + 80, info_y);
  display_.print("Max: ");
  if (thermal_max_sensor_ && thermal_max_sensor_->has_state()) {
    display_.print(thermal_max_sensor_->state, 1);
    display_.print("C");
  } else {
    display_.print("--");
  }

  // Avg temp (next line)
  display_.setCursor(image_x, info_y + 12);
  display_.print("Avg: ");
  if (thermal_avg_sensor_ && thermal_avg_sensor_->has_state()) {
    display_.print(thermal_avg_sensor_->state, 1);
    display_.print("C");
  } else {
    display_.print("--");
  }

  // Draw thermal image with color mapping - using interpolated data for smoother display
  for (int y = 0; y < 48; y++) {
    for (int x = 0; x < 64; x++) {
      int pixel_idx = y * 64 + x;
      float temperature = interpolated_pixels_[pixel_idx];

      // Get color from thermal palette
      uint16_t color = temp_to_color_(temperature, thermal_min_temp_, thermal_max_temp_);

      // Calculate exact pixel dimensions to fill entire area
      int draw_x = image_x + (x * image_w / 64);
      int draw_y = image_y + (y * image_h / 48);
      int next_x = image_x + ((x + 1) * image_w / 64);
      int next_y = image_y + ((y + 1) * image_h / 48);

      // Fill rectangle to next pixel boundary to avoid gaps
      int width = next_x - draw_x;
      int height = next_y - draw_y;

      display_.fillRect(draw_x, draw_y, width, height, color);
    }
  }

  // Draw border around thermal image
  display_.drawRect(image_x - 1, image_y - 1, image_w + 2, image_h + 2, COLOR_TEXT);
}

void GrowEnvMonitor::setup_thermal_web_server_() {
  ESP_LOGCONFIG(TAG, "Setting up thermal image web server...");

  // Skip web server setup for now to avoid crash - will implement simpler approach
  ESP_LOGCONFIG(TAG, "Thermal web server setup skipped - thermal camera functional via display");
}

// Thermal interpolation functions for smooth upscaling
float GrowEnvMonitor::get_point_(float *p, uint8_t rows, uint8_t cols, int8_t x, int8_t y) {
  if (x < 0)
    x = 0;
  if (y < 0)
    y = 0;
  if (x >= cols)
    x = cols - 1;
  if (y >= rows)
    y = rows - 1;
  return p[y * cols + x];
}

void GrowEnvMonitor::set_point_(float *p, uint8_t rows, uint8_t cols, int8_t x, int8_t y, float f) {
  if ((x < 0) || (x >= cols))
    return;
  if ((y < 0) || (y >= rows))
    return;
  p[y * cols + x] = f;
}

void GrowEnvMonitor::get_adjacents_2d_(float *src, float *dest, uint8_t rows, uint8_t cols, int8_t x, int8_t y) {
  for (int8_t delta_y = -1; delta_y < 3; delta_y++) {    // -1, 0, 1, 2
    float *row = dest + 4 * (delta_y + 1);               // index into each chunk of 4
    for (int8_t delta_x = -1; delta_x < 3; delta_x++) {  // -1, 0, 1, 2
      row[delta_x + 1] = get_point_(src, rows, cols, x + delta_x, y + delta_y);
    }
  }
}

float GrowEnvMonitor::cubic_interpolate_(float p[], float x) {
  return p[1] +
         (0.5 * x *
          (p[2] - p[0] + x * (2.0 * p[0] - 5.0 * p[1] + 4.0 * p[2] - p[3] + x * (3.0 * (p[1] - p[2]) + p[3] - p[0]))));
}

float GrowEnvMonitor::bicubic_interpolate_(float p[], float x, float y) {
  float arr[4] = {0, 0, 0, 0};
  arr[0] = cubic_interpolate_(p + 0, x);
  arr[1] = cubic_interpolate_(p + 4, x);
  arr[2] = cubic_interpolate_(p + 8, x);
  arr[3] = cubic_interpolate_(p + 12, x);
  return cubic_interpolate_(arr, y);
}

void GrowEnvMonitor::interpolate_image_(float *src, uint8_t src_rows, uint8_t src_cols, float *dest, uint8_t dest_rows,
                                        uint8_t dest_cols) {
  float mu_x = (src_cols - 1.0) / (dest_cols - 1.0);
  float mu_y = (src_rows - 1.0) / (dest_rows - 1.0);

  float adj_2d[16];  // matrix for storing adjacents

  for (uint8_t y_idx = 0; y_idx < dest_rows; y_idx++) {
    for (uint8_t x_idx = 0; x_idx < dest_cols; x_idx++) {
      float x = x_idx * mu_x;
      float y = y_idx * mu_y;
      get_adjacents_2d_(src, adj_2d, src_rows, src_cols, x, y);
      float frac_x = x - (int) x;  // fractional part between points
      float frac_y = y - (int) y;  // fractional part between points
      float out = bicubic_interpolate_(adj_2d, frac_x, frac_y);
      set_point_(dest, dest_rows, dest_cols, x_idx, y_idx, out);
    }
  }
}

// Selective redraw functions for efficient display updates
void GrowEnvMonitor::check_and_update_sensor_values_() {
  bool changed = false;

  // Check CO2 sensor
  if (co2_sensor_ && co2_sensor_->has_state()) {
    float co2 = co2_sensor_->state;
    if (isnan(prev_co2_) || fabs(co2 - prev_co2_) > 5.0) {  // 5 ppm threshold
      prev_co2_ = co2;
      draw_co2_value_();
      changed = true;
    }
  }

  // Check temperature sensor
  if (temperature_sensor_ && temperature_sensor_->has_state()) {
    float temp = temperature_sensor_->state;
    if (isnan(prev_temperature_) || fabs(temp - prev_temperature_) > 0.1) {  // 0.1°C threshold
      prev_temperature_ = temp;
      draw_temperature_value_();
      changed = true;
    }
  }

  // Check humidity sensor
  if (humidity_sensor_ && humidity_sensor_->has_state()) {
    float humidity = humidity_sensor_->state;
    if (isnan(prev_humidity_) || fabs(humidity - prev_humidity_) > 0.5) {  // 0.5% threshold
      prev_humidity_ = humidity;
      draw_humidity_value_();
      changed = true;
    }
  }

  // Update VPD if temp or humidity changed
  if (changed && temperature_sensor_ && temperature_sensor_->has_state() && humidity_sensor_ &&
      humidity_sensor_->has_state()) {
    draw_vpd_value_();
  }

  // Check light sensor
  if (light_sensor_ && light_sensor_->has_state()) {
    bool light_on = light_sensor_->state;
    if (light_on != prev_light_on_) {
      prev_light_on_ = light_on;
      draw_light_status_();
      changed = true;
    }
  }

  // Check thermal sensor values
  bool thermal_changed = false;
  if (thermal_min_sensor_ && thermal_min_sensor_->has_state()) {
    float thermal_min = thermal_min_sensor_->state;
    if (isnan(prev_thermal_min_) || fabs(thermal_min - prev_thermal_min_) > 0.1) {
      prev_thermal_min_ = thermal_min;
      thermal_changed = true;
    }
  }

  if (thermal_max_sensor_ && thermal_max_sensor_->has_state()) {
    float thermal_max = thermal_max_sensor_->state;
    if (isnan(prev_thermal_max_) || fabs(thermal_max - prev_thermal_max_) > 0.1) {
      prev_thermal_max_ = thermal_max;
      thermal_changed = true;
    }
  }

  if (thermal_avg_sensor_ && thermal_avg_sensor_->has_state()) {
    float thermal_avg = thermal_avg_sensor_->state;
    if (isnan(prev_thermal_avg_) || fabs(thermal_avg - prev_thermal_avg_) > 0.1) {
      prev_thermal_avg_ = thermal_avg;
      thermal_changed = true;
    }
  }

  // Update status if any sensor changed
  if (changed) {
    draw_status_();
  }
}

void GrowEnvMonitor::draw_co2_value_() {
  int y = MARGIN + 30;

  // Clear the CO2 value area
  display_.fillRect(MARGIN + 30, y, 100, LINE_HEIGHT, COLOR_BACKGROUND);

  display_.setTextSize(1);
  display_.setCursor(MARGIN, y);
  display_.setTextColor(COLOR_TEXT);
  display_.print("CO2: ");

  if (co2_sensor_ && co2_sensor_->has_state()) {
    float co2 = co2_sensor_->state;
    uint16_t co2_color = (co2 > 1500) ? COLOR_ALERT : (co2 > 1000) ? COLOR_WARNING : COLOR_GOOD;
    display_.setTextColor(co2_color);
    display_.print(co2, 0);
    display_.setTextColor(COLOR_TEXT);
    display_.print(" ppm");
  } else {
    display_.setTextColor(COLOR_WARNING);
    display_.print("No data");
  }
}

void GrowEnvMonitor::draw_temperature_value_() {
  int y = MARGIN + 55;

  // Clear the temperature value area
  display_.fillRect(MARGIN + 42, y, 80, LINE_HEIGHT, COLOR_BACKGROUND);

  display_.setTextSize(1);
  display_.setCursor(MARGIN, y);
  display_.setTextColor(COLOR_TEXT);
  display_.print("Temp: ");

  if (temperature_sensor_ && temperature_sensor_->has_state()) {
    float temp = temperature_sensor_->state;
    uint16_t temp_color = (temp > 30 || temp < 18)   ? COLOR_ALERT
                          : (temp > 28 || temp < 20) ? COLOR_WARNING
                                                     : COLOR_GOOD;
    display_.setTextColor(temp_color);
    display_.print(temp, 1);
    display_.setTextColor(COLOR_TEXT);
    display_.print(" C");
  } else {
    display_.setTextColor(COLOR_WARNING);
    display_.print("No data");
  }
}

void GrowEnvMonitor::draw_humidity_value_() {
  int y = MARGIN + 80;

  // Clear the humidity value area
  display_.fillRect(MARGIN + 66, y, 80, LINE_HEIGHT, COLOR_BACKGROUND);

  display_.setTextSize(1);
  display_.setCursor(MARGIN, y);
  display_.setTextColor(COLOR_TEXT);
  display_.print("Humidity: ");

  if (humidity_sensor_ && humidity_sensor_->has_state()) {
    float humidity = humidity_sensor_->state;
    uint16_t humidity_color = (humidity > 70 || humidity < 40)   ? COLOR_ALERT
                              : (humidity > 65 || humidity < 45) ? COLOR_WARNING
                                                                 : COLOR_GOOD;
    display_.setTextColor(humidity_color);
    display_.print(humidity, 1);
    display_.setTextColor(COLOR_TEXT);
    display_.print(" %");
  } else {
    display_.setTextColor(COLOR_WARNING);
    display_.print("No data");
  }
}

void GrowEnvMonitor::draw_vpd_value_() {
  int y = MARGIN + 105;

  // Clear the VPD value area
  display_.fillRect(MARGIN + 30, y, 100, LINE_HEIGHT, COLOR_BACKGROUND);

  display_.setTextSize(1);
  display_.setCursor(MARGIN, y);
  display_.setTextColor(COLOR_TEXT);
  display_.print("VPD: ");

  if (temperature_sensor_ && temperature_sensor_->has_state() && humidity_sensor_ && humidity_sensor_->has_state()) {
    float vpd = calculate_vpd_(temperature_sensor_->state, humidity_sensor_->state);
    uint16_t vpd_color = (vpd > 1.5 || vpd < 0.4) ? COLOR_ALERT : (vpd > 1.2 || vpd < 0.6) ? COLOR_WARNING : COLOR_GOOD;
    display_.setTextColor(vpd_color);
    display_.print(vpd, 2);
    display_.setTextColor(COLOR_TEXT);
    display_.print(" kPa");
  } else {
    display_.setTextColor(COLOR_WARNING);
    display_.print("No data");
  }
}

void GrowEnvMonitor::draw_light_status_() {
  int y = MARGIN + 130;

  // Clear the light status area
  display_.fillRect(MARGIN, y, 150, LINE_HEIGHT, COLOR_BACKGROUND);

  display_.setTextSize(1);
  display_.setCursor(MARGIN, y);
  display_.setTextColor(COLOR_TEXT);
  display_.print("Light: ");

  if (light_sensor_ && light_sensor_->has_state()) {
    bool light_on = light_sensor_->state;
    uint16_t light_color = light_on ? COLOR_GOOD : COLOR_WARNING;
    display_.setTextColor(light_color);
    display_.print(light_on ? "ON" : "OFF");
  } else {
    display_.setTextColor(COLOR_WARNING);
    display_.print("No data");
  }
}

void GrowEnvMonitor::draw_thermal_values_() {
  int y = MARGIN + 160;

  // Clear thermal values area
  display_.fillRect(MARGIN, y, 300, LINE_HEIGHT * 3, COLOR_BACKGROUND);

  display_.setTextSize(1);
  display_.setTextColor(COLOR_TEXT);

  // Thermal Min
  display_.setCursor(MARGIN, y);
  display_.print("Min: ");
  if (thermal_min_sensor_ && thermal_min_sensor_->has_state()) {
    display_.print(thermal_min_sensor_->state, 1);
    display_.print(" C");
  } else {
    display_.setTextColor(COLOR_WARNING);
    display_.print("No data");
    display_.setTextColor(COLOR_TEXT);
  }
  y += LINE_HEIGHT;

  // Thermal Max
  display_.setCursor(MARGIN, y);
  display_.print("Max: ");
  if (thermal_max_sensor_ && thermal_max_sensor_->has_state()) {
    display_.print(thermal_max_sensor_->state, 1);
    display_.print(" C");
  } else {
    display_.setTextColor(COLOR_WARNING);
    display_.print("No data");
    display_.setTextColor(COLOR_TEXT);
  }

  // Thermal Average (on same line as Max)
  display_.setCursor(MARGIN + 120, y);
  display_.print("Avg: ");
  if (thermal_avg_sensor_ && thermal_avg_sensor_->has_state()) {
    display_.print(thermal_avg_sensor_->state, 1);
    display_.print(" C");
  } else {
    display_.setTextColor(COLOR_WARNING);
    display_.print("No data");
    display_.setTextColor(COLOR_TEXT);
  }
}

void GrowEnvMonitor::draw_status_() {
  // Status display removed - using detailed alerts section instead
}

// Runtime configuration change handlers
void GrowEnvMonitor::update_thermal_refresh_rate(const std::string &rate) {
  if (!thermal_initialized_)
    return;

  thermal_refresh_rate_ = rate;
  uint16_t rate_code = parse_refresh_rate_(rate);
  int status = MLX90640_SetRefreshRate(MLX90640_ADDRESS, rate_code);
  if (status != 0) {
    ESP_LOGW(TAG, "Failed to update MLX90640 refresh rate: %d", status);
  } else {
    ESP_LOGCONFIG(TAG, "MLX90640 refresh rate updated to %s", rate.c_str());
  }
}

void GrowEnvMonitor::update_thermal_pattern(const std::string &pattern) {
  if (!thermal_initialized_)
    return;

  thermal_pattern_ = pattern;
  int status = setup_thermal_pattern_(pattern);
  if (status != 0) {
    ESP_LOGW(TAG, "Failed to update MLX90640 pattern: %d", status);
  } else {
    ESP_LOGCONFIG(TAG, "MLX90640 pattern updated to %s", pattern.c_str());
  }
}

void GrowEnvMonitor::update_thermal_palette(const std::string &palette) {
  thermal_palette_ = palette;
  set_active_palette_();
  ESP_LOGCONFIG(TAG, "Thermal palette updated to %s", palette.c_str());
}

void GrowEnvMonitor::update_thermal_single_frame(bool single_frame) {
  thermal_single_frame_ = single_frame;
  ESP_LOGCONFIG(TAG, "Thermal single frame mode %s", single_frame ? "enabled" : "disabled");
}

void GrowEnvMonitor::update_thermal_interval(float interval_ms) {
  thermal_update_interval_ = (uint32_t) interval_ms;
  ESP_LOGCONFIG(TAG, "Thermal update interval set to %dms", thermal_update_interval_);
}

}  // namespace grow_env_monitor
}  // namespace esphome