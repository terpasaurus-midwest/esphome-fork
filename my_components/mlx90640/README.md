# MLX90640 Thermal Imaging Component

A standalone ESPHome component exposing and enhancing the Melexis MLX90640 thermal temperature imaging device's capabilities in ESPHome.

The concept is to expose all the useful functionality from the Melexis driver into configurable ESPHome entities. And to provide a framework to build additional features enhancing the implementation.

## Features

This component provides comprehensive MLX90640 thermal imaging capabilities with several unique features not found in existing ESPHome implementations:

### 🌡️ **Complete Thermal Imaging**
- **32×24 pixel thermal array** with temperature range -40°C to 85°C
- **Configurable refresh rates** from 0.5Hz to 64Hz
- **Multiple resolution modes** (16-bit to 19-bit)
- **Chess/Interleaved readout patterns** for optimal performance
- **Bad pixel correction** and frame synchronization
- **Synchronized data acquisition** - no blank or incomplete images due to timing issues 

### 🎯 **Advanced ROI (Region of Interest) Support**
- **Configurable square area** with adjustable position and size within the thermal image
- **Independent temperature statistics** with dedicated sensor entities for ROI min/max/avg temperatures
- **Real-time ROI adjustment** via ESPHome number/switch controls with persistence across reboots
- **Filter out outliers** - exclude excessively hot or cold objects that would invalidate temperature measurements
- **Perfect for agriculture** - target leaf temperatures while excluding hot lights, buds, and equipment

### 🎨 **Configurable Thermographic Color Palettes**
- **Multiple color palette options** (rainbow, golden, grayscale, ironblack, cam, ironbow, arctic, lava, whitehot, blackhot)
- **Runtime palette switching** via drop-down select entity in ESPHome web UI
- **Hot-swappable without restart** - changes apply immediately
- **Persistent across reboots** when configured with restore_value: true
- **Web server JPEG endpoint** for thermal image viewing with selectable quality

### 🔗 **ESPHome Integration**
- **Auto-generated sensors and controls** - no manual template configuration needed
- **Runtime configuration** via web interface with persistent settings
- **Clean API** for use in custom components (e.g., drawing thermal images on displays)
- **Home Assistant integration** ready out of the box

## Hardware Requirements

- **ESP32 microcontroller**
- **MLX90640 thermal camera** connected via I2C
- **I2C connection**: SDA and SCL pins (configurable)
- **3.3V power supply** for MLX90640

## Installation

### Option 1: GitHub Reference
```yaml
external_components:
  - source:
      type: git
      url: https://github.com/terpasaurus-midwest/esphome-fork
      ref: m5stack
    components: [mlx90640]
```

### Option 2: Local Component
```yaml
external_components:
  - source:
      type: local
      path: /path/to/components
    components: [mlx90640]
```

## Basic Configuration

```yaml
# Required I2C bus (M5Stack Core S3 pins used for this example)
i2c:
  sda: 21  # change to match your ESP hardware
  scl: 22  # change to match your ESP hardware
  scan: true

# MLX90640 component
mlx90640:
  id: thermal_camera
  refresh_rate: "16Hz"        # Optional, defaults to 16Hz
  resolution: "18-bit"        # Optional, defaults to 18-bit
  pattern: "chess"            # Optional, defaults to chess
  single_frame: false         # Optional, defaults to false
  update_interval: 2000       # Static value in milliseconds
  
  # Web server for thermal image viewing (optional)
  web_server:
    enable: true
    path: "/thermal.jpg"
    quality: 80               # JPEG quality 10-100
```

## Full Configuration Reference

Everything here is optional/implied, but all options are provided here in case you need to change anything.

Just because you can change something, does not mean it will work. For example:
* 32Hz refresh rate and above is known to make the hardware stop working.
  * Slower refresh rates with faster update intervals provide better temperature accuracy than faster refresh rates. Review the Melexis datasheet before using this product for serious business.
* Melexis did not calibrate the sensor for a resolution other than 18-bit and a pattern other than chess.
* Using single-frame mode can be helpful if you need to prioritize fast motion performance in your image frame, but it will decrease your temperature measurement accuracy. It is not advised for horticulture and similar use.

```yaml
mlx90640:
  id: thermal_camera
  refresh_rate: "16Hz"        # Optional, defaults to 16Hz
  resolution: "18-bit"        # Optional, defaults to 18-bit
  pattern: "chess"            # Optional, defaults to chess
  single_frame: false         # Optional, defaults to false
  update_interval: 2000       # Static value in milliseconds
  
  # Web server for thermal image viewing (optional)
  web_server:
    enable: true
    path: "/thermal.jpg"
    quality: 80               # JPEG quality 10-100
  
  # Auto-generated temperature sensors (optional)
  temperature_sensors:
    min:
      name: "Thermal Min Temperature"
    max:
      name: "Thermal Max Temperature"
    avg:
      name: "Thermal Average Temperature"
    roi_min:
      name: "ROI Min Temperature"
    roi_max:
      name: "ROI Max Temperature"
    roi_avg:
      name: "ROI Average Temperature"
  
  # Auto-generated user controls (optional)
  update_interval_control:
    name: "Thermal Update Interval"
    min_value: 100              # Optional, defaults to 100ms
    max_value: 30000            # Optional, defaults to 30000ms
    restore_value: true         # Optional, enables persistence
    
  thermal_palette_control:
    name: "Thermal Color Palette"
    restore_value: true         # Optional, enables/disables persistence
    
  # ROI configuration (optional)
  roi:
    enabled: false
    center_row: 12
    center_col: 16
    size: 2
    
  # ROI runtime controls (optional)
  roi_enabled_control:
    name: "ROI Enabled"
    restore_mode: RESTORE_DEFAULT_OFF  # Optional, enables persistence
    
  roi_center_row_control:
    name: "ROI Center Row"
    restore_value: true         # Optional, enables persistence
    
  roi_center_col_control:
    name: "ROI Center Column"
    restore_value: true         # Optional, enables persistence
    
  roi_size_control:
    name: "ROI Size"
    restore_value: true         # Optional, enables persistence
```

## Configuration Reference

### Hardware Settings (Static)
- **`refresh_rate`**: `"0.5Hz"` to `"64Hz"` - thermal sensor refresh rate
- **`resolution`**: `"16-bit"` to `"19-bit"` - thermal sensor precision
- **`pattern`**: `"chess"` (recommended) or `"interleaved"` - readout pattern
- **`single_frame`**: `false` (better quality, accuracy) or `true` (faster, less accurate)
- **`update_interval`**: Static update frequency in milliseconds

### Auto-Generated Sensors
Configure `temperature_sensors` with any combination of:
- **`min`**, **`max`**, **`avg`**: Full-frame temperature statistics
- **`roi_min`**, **`roi_max`**, **`roi_avg`**: ROI-specific temperature statistics

Each sensor supports standard sensor options: `name`, `id`, `accuracy_decimals`, etc.

### Auto-Generated Controls
Configure user controls with these options:
- **`update_interval_control`**: Runtime thermal update interval control (Number)
- **`thermal_palette_control`**: Runtime color palette selection (Select)
- **`roi_enabled_control`**: Runtime ROI enable/disable (Switch)
- **`roi_center_row_control`**: Runtime ROI center row adjustment (Number)
- **`roi_center_col_control`**: Runtime ROI center column adjustment (Number)
- **`roi_size_control`**: Runtime ROI size adjustment (Number)

Each control supports standard ESPHome options: `name`, `id`, web_server grouping, etc.

### Persistence Options
- **Number/Select controls**: Use `restore_value: true` to persist changes across reboots
- **Switch controls**: Use `restore_mode: RESTORE_DEFAULT_OFF/ON` to persist state across reboots
- **Default behavior**: Without persistence options, controls revert to YAML-configured initial values on reboot

### ROI Configuration
- **`roi.enabled`**: Initial ROI enable state
- **`roi.center_row`**: Initial ROI center (1-24)
- **`roi.center_col`**: Initial ROI center (1-32)
- **`roi.size`**: Initial ROI size (1-10, creates (2n+1)×(2n+1) square)

### Web Server
- **`web_server.enable`**: Enable thermal image HTTP endpoint
- **`web_server.path`**: Image URL path (default "/thermal.jpg")
- **`web_server.quality`**: JPEG quality 10-100

## API for Custom Components

The MLX90640Component provides a clean API for integration with other components:

```cpp
#include "esphome/components/mlx90640/mlx90640.h"

// Access thermal data
auto thermal = id(thermal_camera);
const float* pixels = thermal->get_thermal_pixels();      // Raw 32×24 array
const float* smooth = thermal->get_interpolated_pixels(); // Smooth 64×48 array

// Get temperature statistics
float min_temp = thermal->get_min_temp();
float max_temp = thermal->get_max_temp();
float avg_temp = thermal->get_avg_temp();

// ROI data (when enabled)
if (thermal->is_roi_enabled()) {
  float roi_avg = thermal->get_roi_avg_temp();
  int roi_pixels = thermal->get_roi_pixel_count();
}
```

## License

This component includes the official Melexis MLX90640 API under Apache License 2.0.

## Contributing

Contributions welcome! This component is designed to be hardware-agnostic and easily extensible.
