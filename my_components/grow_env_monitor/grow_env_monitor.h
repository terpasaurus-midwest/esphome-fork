#pragma once

#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/binary_sensor/binary_sensor.h"
#include <M5GFX.h>
#include "MLX90640_API.h"
#include "MLX90640_I2C_Driver.h"
#include <Wire.h>

namespace esphome {
namespace grow_env_monitor {

class GrowEnvMonitor : public Component {
 public:
  void setup() override;
  void loop() override;
  void dump_config() override;

  void set_display_brightness(int brightness) { display_brightness_ = brightness; }
  void set_display_rotation(int rotation) { display_rotation_ = rotation; }

  void set_co2_sensor(sensor::Sensor *co2_sensor) { co2_sensor_ = co2_sensor; }
  void set_temperature_sensor(sensor::Sensor *temperature_sensor) { temperature_sensor_ = temperature_sensor; }
  void set_humidity_sensor(sensor::Sensor *humidity_sensor) { humidity_sensor_ = humidity_sensor; }
  void set_thermal_min_sensor(sensor::Sensor *thermal_min_sensor) { thermal_min_sensor_ = thermal_min_sensor; }
  void set_thermal_max_sensor(sensor::Sensor *thermal_max_sensor) { thermal_max_sensor_ = thermal_max_sensor; }
  void set_thermal_avg_sensor(sensor::Sensor *thermal_avg_sensor) { thermal_avg_sensor_ = thermal_avg_sensor; }
  void set_light_sensor(binary_sensor::BinarySensor *light_sensor) { light_sensor_ = light_sensor; }

  // Thermal camera configuration setters
  void set_thermal_refresh_rate(const std::string &rate) { thermal_refresh_rate_ = rate; }
  void set_thermal_resolution(const std::string &resolution) { thermal_resolution_ = resolution; }
  void set_thermal_pattern(const std::string &pattern) { thermal_pattern_ = pattern; }
  void set_thermal_palette(const std::string &palette) { thermal_palette_ = palette; }
  void set_thermal_single_frame(bool single_frame) { thermal_single_frame_ = single_frame; }

  // Runtime configuration change handlers
  void update_thermal_refresh_rate(const std::string &rate);
  void update_thermal_pattern(const std::string &pattern);
  void update_thermal_palette(const std::string &palette);
  void update_thermal_single_frame(bool single_frame);
  void update_thermal_interval(float interval_ms);

 protected:
  void draw_screen_();
  void draw_sensor_data_();
  void draw_thermal_data_();
  void draw_thermal_image_();
  void draw_alerts_();

  // Selective redraw functions
  void draw_co2_value_();
  void draw_temperature_value_();
  void draw_humidity_value_();
  void draw_vpd_value_();
  void draw_light_status_();
  void draw_thermal_values_();
  void draw_status_();
  void check_and_update_sensor_values_();
  float calculate_vpd_(float temperature, float humidity);
  uint16_t get_status_color_(float co2, float temp, float humidity);
  void setup_thermal_();
  void update_thermal_data_();

  M5GFX display_;
  bool initialized_{false};
  int display_brightness_{70};
  int display_rotation_{3};

  sensor::Sensor *co2_sensor_{nullptr};
  sensor::Sensor *temperature_sensor_{nullptr};
  sensor::Sensor *humidity_sensor_{nullptr};
  sensor::Sensor *thermal_min_sensor_{nullptr};
  sensor::Sensor *thermal_max_sensor_{nullptr};
  sensor::Sensor *thermal_avg_sensor_{nullptr};
  binary_sensor::BinarySensor *light_sensor_{nullptr};

  // Previous values for selective redrawing
  float prev_co2_{NAN};
  float prev_temperature_{NAN};
  float prev_humidity_{NAN};
  float prev_thermal_min_{NAN};
  float prev_thermal_max_{NAN};
  float prev_thermal_avg_{NAN};
  bool prev_light_on_{false};

  // Thermal camera configuration
  std::string thermal_refresh_rate_{"16Hz"};  // Balanced performance default
  std::string thermal_resolution_{"18-bit"};
  std::string thermal_pattern_{"chess"};
  std::string thermal_palette_{"rainbow"};
  bool thermal_single_frame_{false};  // Read single frame for motion handling
  const uint16_t *current_palette_{nullptr};

  // MLX90640 thermal camera
  static const uint8_t MLX90640_ADDRESS = 0x33;
  static const int TA_SHIFT = 8;
  paramsMLX90640 mlx90640_params_;
  float mlx90640_pixels_[32 * 24];
  float interpolated_pixels_[64 * 48];  // 2x upscaled thermal data
  bool thermal_initialized_{false};

  // Thermal temperature data
  float thermal_min_temp_{20.0};
  float thermal_max_temp_{30.0};
  float thermal_avg_temp_{25.0};
  float thermal_median_temp_{25.0};

  // Thermal image generation and configuration
  static const uint16_t thermal_palette_rainbow_[256];
  static const uint16_t thermal_palette_golden_[256];
  static const uint16_t thermal_palette_grayscale_[256];
  static const uint16_t thermal_palette_ironblack_[256];
  static const uint16_t thermal_palette_cam_[256];
  static const uint16_t thermal_palette_ironbow_[256];
  static const uint16_t thermal_palette_arctic_[256];
  static const uint16_t thermal_palette_lava_[256];
  static const uint16_t thermal_palette_whitehot_[256];
  static const uint16_t thermal_palette_blackhot_[256];

  void generate_thermal_image_(uint8_t *rgb_buffer);
  uint16_t temp_to_color_(float temperature, float min_temp, float max_temp);
  void setup_thermal_web_server_();

  // Configuration helper functions
  uint16_t parse_refresh_rate_(const std::string &rate_str);
  int parse_resolution_(const std::string &res_str);
  int setup_thermal_resolution_(int bits);
  int setup_thermal_pattern_(const std::string &pattern);
  void set_active_palette_();

  // Thermal interpolation functions
  float get_point_(float *p, uint8_t rows, uint8_t cols, int8_t x, int8_t y);
  void set_point_(float *p, uint8_t rows, uint8_t cols, int8_t x, int8_t y, float f);
  void get_adjacents_2d_(float *src, float *dest, uint8_t rows, uint8_t cols, int8_t x, int8_t y);
  float cubic_interpolate_(float p[], float x);
  float bicubic_interpolate_(float p[], float x, float y);
  void interpolate_image_(float *src, uint8_t src_rows, uint8_t src_cols, float *dest, uint8_t dest_rows,
                          uint8_t dest_cols);

  // Update tracking
  uint32_t last_update_time_{0};
  uint32_t last_thermal_update_time_{0};
  uint32_t thermal_update_interval_{20000};       // Default 20 seconds, user configurable
  static const uint32_t UPDATE_INTERVAL = 10000;  // 10 seconds for environmental data

  // Display layout constants
  static const int SCREEN_WIDTH = 320;
  static const int SCREEN_HEIGHT = 240;
  static const int MARGIN = 10;
  static const int LINE_HEIGHT = 25;

  // Color constants
  static const uint16_t COLOR_BACKGROUND = 0x0000;  // Black
  static const uint16_t COLOR_TEXT = 0xFFFF;        // White
  static const uint16_t COLOR_GOOD = 0x07E0;        // Green
  static const uint16_t COLOR_WARNING = 0xFFE0;     // Yellow
  static const uint16_t COLOR_ALERT = 0xF800;       // Red
  static const uint16_t COLOR_HEADER = 0x07FF;      // Cyan
};

}  // namespace grow_env_monitor
}  // namespace esphome
