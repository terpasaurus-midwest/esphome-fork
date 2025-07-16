#include "esphome/core/log.h"
#include "m5cores3_display.h"
#include "../m5unit_co2l/m5unit_co2l.h"
#include <cmath>

namespace esphome {
namespace m5cores3_display {

static const char *TAG = "m5cores3_display";

void M5CoreS3Display::setup() {
  ESP_LOGCONFIG(TAG, "Setting up M5CoreS3 Display...");

  display_.begin();
  display_.setRotation(3);
  display_.fillScreen(TFT_WHITE);
  display_.setBrightness(70);
  display_.setFont(&fonts::Font4);
  display_.setTextColor(TFT_BLACK);
  display_.setTextSize(1);

  initialized_ = true;
  ESP_LOGCONFIG(TAG, "M5CoreS3 Display setup complete");
}

void M5CoreS3Display::loop() {
  if (!initialized_)
    return;

  // Check if we need to update the display
  uint32_t now = millis();
  if (now - last_update_time_ > 1000) {  // Check every second
    bool should_update = false;

    if (co2l_component_ != nullptr) {
      auto &unit = co2l_component_->unit_;

      // Update if sensor data has been updated or if it's been too long
      if (unit.updated() || display_needs_update_ || now - last_update_time_ > 10000) {
        should_update = true;
        display_needs_update_ = false;
        ESP_LOGD(TAG, "Display update - CO2: %u, Temp: %.1f, Humidity: %.1f", unit.co2(), unit.temperature(),
                 unit.humidity());
      }
    } else {
      // No sensor, but still update periodically
      if (now - last_update_time_ > 10000) {
        should_update = true;
      }
    }

    if (should_update) {
      draw_environmental_data_();
      last_update_time_ = now;
    }
  }
}

void M5CoreS3Display::draw_environmental_data_() {
  // Clear the screen
  display_.fillScreen(TFT_WHITE);

  // Set text properties
  display_.setFont(&fonts::Font4);  // Smaller font to fit more content
  display_.setTextColor(TFT_BLACK);
  display_.setTextSize(1);
  display_.setTextDatum(TL_DATUM);  // Top Left alignment

  int y_pos = 10;
  int line_height = display_.fontHeight() + 8;      // Compact spacing between sections
  int label_value_gap = display_.fontHeight() + 1;  // Minimal gap between label and value

  // Get data from CO2L component if available
  if (co2l_component_ != nullptr) {
    // Access the unit directly to get current readings
    auto &unit = co2l_component_->unit_;

    // CO2 reading - vertical format
    display_.drawString("CO2", 10, y_pos);
    y_pos += label_value_gap;
    String co2_value;
    if (unit.updated() || unit.co2() > 0) {
      co2_value = String(unit.co2()) + " ppm";
    } else {
      co2_value = "--- ppm";
    }
    display_.drawString(co2_value, 10, y_pos);
    y_pos += line_height;

    // Temperature reading - vertical format
    display_.drawString("Temp", 10, y_pos);
    y_pos += label_value_gap;
    if (unit.updated() || !isnan(unit.temperature())) {
      String temp_value = String(unit.temperature(), 1);
      int temp_width = display_.textWidth(temp_value.c_str());
      display_.drawString(temp_value.c_str(), 10, y_pos);
      // Draw a small circle for degree symbol
      display_.drawCircle(10 + temp_width + 2, y_pos + 2, 2, TFT_BLACK);
      display_.drawString("C", 10 + temp_width + 8, y_pos);
    } else {
      int temp_width = display_.textWidth("---");
      display_.drawString("---", 10, y_pos);
      display_.drawCircle(10 + temp_width + 2, y_pos + 2, 2, TFT_BLACK);
      display_.drawString("C", 10 + temp_width + 8, y_pos);
    }
    y_pos += line_height;

    // Humidity reading - vertical format
    display_.drawString("RH", 10, y_pos);
    y_pos += label_value_gap;
    String hum_value;
    if (unit.updated() || !isnan(unit.humidity())) {
      hum_value = String(unit.humidity(), 1) + " %";
    } else {
      hum_value = "--- %";
    }
    display_.drawString(hum_value, 10, y_pos);
    y_pos += line_height;

    // VPD calculation - vertical format
    display_.drawString("VPD", 10, y_pos);
    y_pos += label_value_gap;
    String vpd_value;
    if (!isnan(unit.temperature()) && !isnan(unit.humidity())) {
      float temp = unit.temperature();
      float humidity = unit.humidity();
      float svp = 0.61078f * exp((17.27f * temp) / (temp + 237.3f));
      float vpd = svp * (1.0f - (humidity / 100.0f));
      vpd_value = String(vpd, 2) + " kPa";
    } else {
      vpd_value = "--- kPa";
    }
    display_.drawString(vpd_value, 10, y_pos);

  } else {
    // No component connected
    display_.setTextColor(TFT_RED);
    display_.drawString("No CO2L sensor", 10, y_pos);
  }

  // Draw thermal camera on the right side
  draw_thermal_camera_();
}

void M5CoreS3Display::draw_thermal_camera_() {
  // Position thermal camera on the right side
  int thermal_x = 160;  // Right side of 320px screen
  int thermal_y = 20;   // Top position

  // Scale the 32x24 image to 128x96 (4x scaling)
  int scaled_width = 128;  // 32 * 4
  int scaled_height = 96;  // 24 * 4

  // Create WiFi client for HTTP request
  WiFiClient client;

  if (client.connect("192.168.8.226", 80)) {
    // Send HTTP GET request
    client.print("GET /thermal-camera HTTP/1.1\r\n");
    client.print("Host: 192.168.8.226\r\n");
    client.print("Connection: close\r\n\r\n");

    // Wait for response
    unsigned long timeout = millis() + 5000;
    while (client.available() == 0) {
      if (millis() > timeout) {
        ESP_LOGE(TAG, "HTTP request timeout");
        client.stop();
        draw_camera_offline_placeholder_(thermal_x, thermal_y, scaled_width, scaled_height);
        return;
      }
      delay(1);
    }

    // Read response headers
    String response = "";
    int content_length = 0;
    bool headers_done = false;

    while (client.available() && !headers_done) {
      String line = client.readStringUntil('\n');
      line.trim();

      if (line.length() == 0) {
        headers_done = true;
      } else if (line.startsWith("Content-Length:")) {
        content_length = line.substring(15).toInt();
      }
    }

    // Read image data
    if (content_length > 0 && content_length < 10000) {
      uint8_t *buffer = (uint8_t *) malloc(content_length);
      if (buffer) {
        int bytes_read = 0;
        while (bytes_read < content_length && client.available()) {
          int available = client.available();
          int to_read = min(available, content_length - bytes_read);
          client.readBytes(buffer + bytes_read, to_read);
          bytes_read += to_read;
        }

        if (bytes_read == content_length && content_length > 100) {  // Ensure we have reasonable data
          // Clear thermal area before drawing new image
          display_.fillRect(thermal_x, thermal_y, scaled_width, scaled_height, TFT_WHITE);

          // Try to draw the BMP from memory with scaling
          bool success = false;
          if (display_.drawBmp(buffer, content_length, thermal_x, thermal_y, scaled_width, scaled_height, 0, 0, 4.0f,
                               4.0f)) {
            ESP_LOGD(TAG, "Thermal camera image displayed successfully (scaled)");
            success = true;
          } else if (display_.drawBmp(buffer, content_length, thermal_x, thermal_y, scaled_width, scaled_height)) {
            ESP_LOGD(TAG, "Thermal camera image displayed successfully (sized)");
            success = true;
          } else if (display_.drawBmp(buffer, content_length, thermal_x, thermal_y)) {
            ESP_LOGD(TAG, "Thermal camera image displayed successfully (original size)");
            success = true;
          }

          if (success) {
            has_valid_thermal_image_ = true;
          } else {
            ESP_LOGD(TAG, "Failed to decode BMP image - keeping last image");
            if (!has_valid_thermal_image_) {
              draw_camera_offline_placeholder_(thermal_x, thermal_y, scaled_width, scaled_height);
            }
          }
        } else {
          ESP_LOGE(TAG, "Failed to read complete image data (%d/%d bytes) - keeping last image", bytes_read,
                   content_length);
          // Don't clear the display, keep the last good image
          if (!has_valid_thermal_image_) {
            draw_camera_offline_placeholder_(thermal_x, thermal_y, scaled_width, scaled_height);
          }
        }

        free(buffer);
      } else {
        ESP_LOGE(TAG, "Failed to allocate memory for image - keeping last image");
        if (!has_valid_thermal_image_) {
          draw_camera_offline_placeholder_(thermal_x, thermal_y, scaled_width, scaled_height);
        }
      }
    } else {
      ESP_LOGE(TAG, "Invalid content length: %d - keeping last image", content_length);
      if (!has_valid_thermal_image_) {
        draw_camera_offline_placeholder_(thermal_x, thermal_y, scaled_width, scaled_height);
      }
    }

    client.stop();
  } else {
    ESP_LOGE(TAG, "Failed to connect to thermal camera - keeping last image");
    if (!has_valid_thermal_image_) {
      draw_camera_offline_placeholder_(thermal_x, thermal_y, scaled_width, scaled_height);
    }
  }
}

void M5CoreS3Display::draw_camera_offline_placeholder_(int x, int y, int width, int height) {
  // Draw a placeholder box
  display_.setTextColor(TFT_RED);
  display_.drawRect(x, y, width, height, TFT_RED);
  display_.drawString("Camera", x + 10, y + 30);
  display_.drawString("Offline", x + 10, y + 50);
}

void M5CoreS3Display::dump_config() {
  ESP_LOGCONFIG(TAG, "M5CoreS3 Display:");
  ESP_LOGCONFIG(TAG, "  Width: %d", initialized_ ? display_.width() : 320);
  ESP_LOGCONFIG(TAG, "  Height: %d", initialized_ ? display_.height() : 240);
  ESP_LOGCONFIG(TAG, "  CO2L Component: %s", co2l_component_ != nullptr ? "Connected" : "Not connected");
}

}  // namespace m5cores3_display
}  // namespace esphome
