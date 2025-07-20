# MLX90640 Thermal Camera Component for ESPHome

A standalone ESPHome component for the Melexis MLX90640 thermal imaging camera with advanced ROI (Region of Interest) functionality.

## Features

### 🌡️ **Complete Thermal Imaging**
- **32×24 pixel thermal array** with temperature range -40°C to 85°C
- **Configurable refresh rates** from 0.5Hz to 64Hz
- **Multiple resolution modes** (16-bit to 19-bit)
- **Chess/Interleaved readout patterns** for optimal performance
- **Bad pixel correction** and frame synchronization

### 🎯 **ROI (Region of Interest) Support**
- **Precise area selection** using 1-based coordinates (1-24 rows, 1-32 columns)
- **Configurable ROI size** with scaling factor (1-10 pixels around center)
- **Independent temperature statistics** for ROI vs full-frame
- **Real-time ROI adjustment** via ESPHome number/switch controls
- **Perfect for agriculture** - target leaf temperatures while excluding hot objects

### 🔗 **ESPHome Integration**
- **Template sensor support** for Home Assistant integration
- **Runtime configuration** via web interface
- **Memory optimized** with stack overflow protection
- **Clean API** for use in other components

## Hardware Requirements

- **ESP32 microcontroller** (any variant)
- **MLX90640 thermal camera** connected via I2C
- **I2C connection**: SDA and SCL pins (configurable)
- **3.3V power supply** for MLX90640 (typically 200mA+)

## Installation

### Option 1: Local Component
```yaml
external_components:
  - source:
      type: local
      path: /path/to/components
    components: [mlx90640]
```

### Option 2: GitHub Reference
```yaml
external_components:
  - source:
      type: git
      url: https://github.com/your-repo/esphome-components
    components: [mlx90640]
```

## Basic Configuration

```yaml
# Required I2C bus
i2c:
  sda: 21
  scl: 22
  scan: true

# Template sensors for thermal data
sensor:
  - platform: template
    name: "Thermal Min Temperature"
    id: thermal_min
    unit_of_measurement: "°C"
    device_class: temperature
    state_class: measurement

  - platform: template
    name: "Thermal Max Temperature"
    id: thermal_max
    unit_of_measurement: "°C"
    device_class: temperature
    state_class: measurement

  - platform: template
    name: "Thermal Average Temperature"
    id: thermal_avg
    unit_of_measurement: "°C"
    device_class: temperature
    state_class: measurement

# MLX90640 component
mlx90640:
  id: thermal_camera
  refresh_rate: "16Hz"
  resolution: "18-bit"
  pattern: "chess"
  single_frame: false
  update_interval: 10000  # 10 seconds
  
  # Optional: Wire up sensors
  temperature_min: thermal_min
  temperature_max: thermal_max
  temperature_avg: thermal_avg
```

## ROI Configuration

Perfect for agricultural applications like cannabis growing where you need to measure leaf temperatures while excluding hot buds:

```yaml
mlx90640:
  id: thermal_camera
  # ... basic config ...
  
  # ROI for targeting specific areas
  roi:
    enabled: true
    center_row: 12        # Center of thermal array (1-24)
    center_col: 16        # Center of thermal array (1-32)
    size: 3               # Creates 7×7 ROI (2*3+1)
  
  # ROI sensors
  roi_min: roi_min_temp
  roi_max: roi_max_temp
  roi_avg: roi_avg_temp

# ROI template sensors
sensor:
  - platform: template
    name: "ROI Min Temperature"
    id: roi_min_temp
    unit_of_measurement: "°C"
    device_class: temperature
```

## Runtime Controls

Add number and switch controls for dynamic ROI adjustment:

```yaml
number:
  - platform: template
    name: "ROI Center Row"
    min_value: 1
    max_value: 24
    step: 1
    initial_value: 12
    optimistic: true
    on_value:
      - lambda: "id(thermal_camera).update_roi_center_row((int)x);"

  - platform: template
    name: "ROI Center Column"
    min_value: 1
    max_value: 32
    step: 1
    initial_value: 16
    optimistic: true
    on_value:
      - lambda: "id(thermal_camera).update_roi_center_col((int)x);"

  - platform: template
    name: "ROI Size"
    min_value: 1
    max_value: 10
    step: 1
    initial_value: 3
    optimistic: true
    on_value:
      - lambda: "id(thermal_camera).update_roi_size((int)x);"

switch:
  - platform: template
    name: "ROI Enabled"
    optimistic: true
    on_turn_on:
      - lambda: "id(thermal_camera).update_roi_enabled(true);"
    on_turn_off:
      - lambda: "id(thermal_camera).update_roi_enabled(false);"
```

## Configuration Options

### Basic Settings
- **`refresh_rate`**: `"0.5Hz"`, `"1Hz"`, `"2Hz"`, `"4Hz"`, `"8Hz"`, `"16Hz"`, `"32Hz"`, `"64Hz"`
- **`resolution`**: `"16-bit"`, `"17-bit"`, `"18-bit"`, `"19-bit"`
- **`pattern`**: `"chess"` (recommended), `"interleaved"`
- **`single_frame`**: `true` (faster, checkerboard), `false` (better quality)
- **`update_interval`**: Update frequency in milliseconds

### ROI Settings
- **`roi.enabled`**: Enable/disable ROI processing
- **`roi.center_row`**: ROI center row (1-24)
- **`roi.center_col`**: ROI center column (1-32) 
- **`roi.size`**: ROI scaling factor (1-10, creates (2n+1)×(2n+1) square)

### Sensor Connections
- **`temperature_min/max/avg`**: Full-frame temperature sensors
- **`roi_min/max/avg`**: ROI-specific temperature sensors

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

## Performance Notes

- **Memory Usage**: ~8KB RAM for buffers (stack-safe)
- **I2C Speed**: Uses standard 400kHz I2C
- **CPU Usage**: Minimal, most processing in hardware
- **Refresh Rates**: Higher rates require more I2C bandwidth
- **ROI Processing**: Adds <1ms per update

## Agricultural Use Case

Perfect for cannabis cultivation and other precision agriculture:

```yaml
# Target leaf canopy temperatures for VPD calculations
mlx90640:
  roi:
    enabled: true
    center_row: 15      # Target middle-lower canopy
    center_col: 20      # Avoid hot lights/equipment
    size: 4             # 9×9 area for good sampling
```

This allows precise leaf temperature measurement while excluding hot buds, lights, or equipment that would skew temperature readings for transpiration calculations.

## Troubleshooting

### No thermal readings
- Check I2C wiring and address (0x33)
- Verify 3.3V power supply (adequate current)
- Enable I2C scanning: `scan: true`

### Poor temperature accuracy
- Allow warm-up time (2-3 minutes)
- Check ambient temperature stability
- Verify emissivity setting (default 0.95)

### High memory usage
- Component uses class member arrays (stack-safe)
- Disable interpolation if not needed for display
- Reduce update frequency for lower I2C usage

## License

This component includes the official Melexis MLX90640 API under Apache License 2.0.

## Contributing

Contributions welcome! This component is designed to be hardware-agnostic and easily extensible.