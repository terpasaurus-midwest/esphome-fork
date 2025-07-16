#pragma once

#include "esphome/core/component.h"
#include <WiFi.h>
#include <M5GFX.h>

// Forward declaration
namespace esphome {
namespace m5unit_co2l {
class M5UnitCO2L;
}
}  // namespace esphome

namespace esphome {
namespace m5cores3_display {

class M5CoreS3Display : public Component {
 public:
  void setup() override;
  void loop() override;
  void dump_config() override;

  void set_co2l_component(m5unit_co2l::M5UnitCO2L *co2l_component) { co2l_component_ = co2l_component; }

 protected:
  void draw_environmental_data_();
  void draw_thermal_camera_();
  void draw_camera_offline_placeholder_(int x, int y, int width, int height);

  M5GFX display_;
  bool initialized_{false};
  m5unit_co2l::M5UnitCO2L *co2l_component_{nullptr};

  // Track last update to avoid unnecessary redraws
  uint32_t last_update_time_{0};
  bool display_needs_update_{true};

  // Track thermal image state
  bool has_valid_thermal_image_{false};
};

}  // namespace m5cores3_display
}  // namespace esphome