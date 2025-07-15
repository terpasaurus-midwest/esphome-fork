#include "esphome/core/log.h"
#include "esphome/core/application.h"
#include "board_m5cores3.h"

namespace esphome {
namespace board_m5cores3 {

static const char *TAG = "board.m5cores3";

void BoardM5CoreS3::setup() {
  ESP_LOGI(TAG, "setup");

  // Give time for serial connection to be established after reset
  ESP_LOGI(TAG, "Waiting 10 seconds for serial connection...");
  for (int i = 0; i < 100; i++) {
    delay(100);      // 100ms delays to avoid watchdog timeout
    App.feed_wdt();  // Feed the watchdog
  }
  ESP_LOGI(TAG, "Starting M5 initialization");

  M5.begin();
  ESP_LOGI(TAG, "M5 system initialized");

  ESP_LOGI(TAG, "Enabling 5V power output on the external ports");
  M5.Power.setExtOutput(true);

  ESP_LOGI(TAG, "Turning off display panel backlight");
  M5.Display.setBrightness(0);
}

void BoardM5CoreS3::loop() {}

void BoardM5CoreS3::dump_config() { ESP_LOGCONFIG(TAG, "config"); }

}  // namespace board_m5cores3
}  // namespace esphome
