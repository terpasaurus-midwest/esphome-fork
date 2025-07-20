import esphome.codegen as cg
from esphome.components import binary_sensor, sensor
import esphome.config_validation as cv
from esphome.const import CONF_ID

DEPENDENCIES = ["esp32"]

CONF_DISPLAY_BRIGHTNESS = "display_brightness"
CONF_DISPLAY_ROTATION = "display_rotation"
CONF_SENSORS = "sensors"
CONF_CO2 = "co2"
CONF_TEMPERATURE = "temperature"
CONF_HUMIDITY = "humidity"
CONF_THERMAL_MIN = "thermal_min"
CONF_THERMAL_MAX = "thermal_max"
CONF_THERMAL_AVG = "thermal_avg"
CONF_ROI_MIN = "roi_min"
CONF_ROI_MAX = "roi_max"
CONF_ROI_AVG = "roi_avg"
CONF_LIGHT_SENSOR = "light_sensor"
CONF_THERMAL_CAMERA = "thermal_camera"
CONF_THERMAL_REFRESH_RATE = "thermal_refresh_rate"
CONF_THERMAL_RESOLUTION = "thermal_resolution"
CONF_THERMAL_PATTERN = "thermal_pattern"
CONF_THERMAL_PALETTE = "thermal_palette"
CONF_THERMAL_SINGLE_FRAME = "thermal_single_frame"
CONF_ROI = "roi"
CONF_ROI_ENABLED = "enabled"
CONF_ROI_CENTER_ROW = "center_row"
CONF_ROI_CENTER_COL = "center_col"
CONF_ROI_SIZE = "size"

# Thermal camera configuration documentation:
#
# THERMAL PATTERNS:
# - "chess": Chess pattern readout (default) - vendor recommended, better for even temperature distribution
# - "interleaved": Interleaved pattern readout - not vendor recommended, different readout pattern
#
# THERMAL PALETTES:
# - "rainbow": Rainbow color palette (default) - most intuitive color mapping, blue=cold, red=hot
# - "golden": Golden color palette - warm tones, good for general thermal viewing
# - "grayscale": Grayscale palette - professional look, good for analysis
# - "ironblack": Iron-black palette - high contrast, good for highlighting temperature differences
# - "cam": CAM palette - specialized thermal camera palette
# - "ironbow": FLIR Ironbow palette - black through blue, magenta, orange, yellow to white
# - "arctic": FLIR Arctic palette - blues for cold to golden yellows/red for warm
# - "lava": FLIR Lava palette - similar to ironbow, hot objects in warm colors, cold in blue
# - "whitehot": FLIR White Hot palette - grayscale with hot=white, cold=black
# - "blackhot": FLIR Black Hot palette - inverted grayscale with hot=black, cold=white
#
# THERMAL REFRESH RATES:
# - Lower rates (0.5Hz-8Hz): Better for static scenes, less I2C bus load
# - Higher rates (16Hz-64Hz): Better for motion detection, requires more I2C bandwidth
# - Note: System automatically adjusts update intervals based on refresh rate
#
# THERMAL SINGLE FRAME:
# - false (default): Read both subpages for better image quality
# - true: Read only one frame to reduce motion artifacts (checkerboard pattern)

grow_env_monitor_ns = cg.esphome_ns.namespace("grow_env_monitor")
GrowEnvMonitor = grow_env_monitor_ns.class_("GrowEnvMonitor", cg.Component)

# Import MLX90640 component
mlx90640_ns = cg.esphome_ns.namespace("mlx90640_app")
MLX90640 = mlx90640_ns.class_("MLX90640")

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(GrowEnvMonitor),
        cv.Optional(CONF_DISPLAY_BRIGHTNESS, default=70): cv.int_range(min=0, max=100),
        cv.Optional(CONF_DISPLAY_ROTATION, default=3): cv.int_range(min=0, max=3),
        cv.Required(CONF_SENSORS): cv.Schema(
            {
                cv.Required(CONF_CO2): cv.use_id(sensor.Sensor),
                cv.Required(CONF_TEMPERATURE): cv.use_id(sensor.Sensor),
                cv.Required(CONF_HUMIDITY): cv.use_id(sensor.Sensor),
                cv.Required(CONF_THERMAL_MIN): cv.use_id(sensor.Sensor),
                cv.Required(CONF_THERMAL_MAX): cv.use_id(sensor.Sensor),
                cv.Required(CONF_THERMAL_AVG): cv.use_id(sensor.Sensor),
                cv.Optional(CONF_ROI_MIN): cv.use_id(sensor.Sensor),
                cv.Optional(CONF_ROI_MAX): cv.use_id(sensor.Sensor),
                cv.Optional(CONF_ROI_AVG): cv.use_id(sensor.Sensor),
                cv.Optional(CONF_LIGHT_SENSOR): cv.use_id(binary_sensor.BinarySensor),
            }
        ),
        cv.Optional("thermal_camera"): cv.Schema(
            {
                cv.Optional(CONF_THERMAL_REFRESH_RATE, default="16Hz"): cv.one_of(
                    "0.5Hz", "1Hz", "2Hz", "4Hz", "8Hz", "16Hz", "32Hz", "64Hz"
                ),
                cv.Optional(CONF_THERMAL_RESOLUTION, default="18-bit"): cv.one_of(
                    "16-bit", "17-bit", "18-bit", "19-bit"
                ),
                cv.Optional(CONF_THERMAL_PATTERN, default="chess"): cv.one_of(
                    "chess", "interleaved"
                ),
                cv.Optional(CONF_THERMAL_PALETTE, default="rainbow"): cv.one_of(
                    "rainbow",
                    "golden",
                    "grayscale",
                    "ironblack",
                    "cam",
                    "ironbow",
                    "arctic",
                    "lava",
                    "whitehot",
                    "blackhot",
                ),
                cv.Optional(CONF_THERMAL_SINGLE_FRAME, default=False): cv.boolean,
            }
        ),
        cv.Optional(CONF_ROI): cv.Schema(
            {
                cv.Optional(CONF_ROI_ENABLED, default=False): cv.boolean,
                cv.Optional(CONF_ROI_CENTER_ROW, default=12): cv.int_range(
                    min=1, max=24
                ),
                cv.Optional(CONF_ROI_CENTER_COL, default=16): cv.int_range(
                    min=1, max=32
                ),
                cv.Optional(CONF_ROI_SIZE, default=2): cv.int_range(min=1, max=10),
            }
        ),
    }
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)

    # Set display configuration
    cg.add(var.set_display_brightness(config[CONF_DISPLAY_BRIGHTNESS]))
    cg.add(var.set_display_rotation(config[CONF_DISPLAY_ROTATION]))

    # Set sensor references
    sensors = config[CONF_SENSORS]

    co2_sensor = await cg.get_variable(sensors[CONF_CO2])
    cg.add(var.set_co2_sensor(co2_sensor))

    temperature_sensor = await cg.get_variable(sensors[CONF_TEMPERATURE])
    cg.add(var.set_temperature_sensor(temperature_sensor))

    humidity_sensor = await cg.get_variable(sensors[CONF_HUMIDITY])
    cg.add(var.set_humidity_sensor(humidity_sensor))

    thermal_min_sensor = await cg.get_variable(sensors[CONF_THERMAL_MIN])
    cg.add(var.set_thermal_min_sensor(thermal_min_sensor))

    thermal_max_sensor = await cg.get_variable(sensors[CONF_THERMAL_MAX])
    cg.add(var.set_thermal_max_sensor(thermal_max_sensor))

    thermal_avg_sensor = await cg.get_variable(sensors[CONF_THERMAL_AVG])
    cg.add(var.set_thermal_avg_sensor(thermal_avg_sensor))

    if CONF_ROI_MIN in sensors:
        roi_min_sensor = await cg.get_variable(sensors[CONF_ROI_MIN])
        cg.add(var.set_roi_min_sensor(roi_min_sensor))

    if CONF_ROI_MAX in sensors:
        roi_max_sensor = await cg.get_variable(sensors[CONF_ROI_MAX])
        cg.add(var.set_roi_max_sensor(roi_max_sensor))

    if CONF_ROI_AVG in sensors:
        roi_avg_sensor = await cg.get_variable(sensors[CONF_ROI_AVG])
        cg.add(var.set_roi_avg_sensor(roi_avg_sensor))

    if CONF_LIGHT_SENSOR in sensors:
        light_sensor = await cg.get_variable(sensors[CONF_LIGHT_SENSOR])
        cg.add(var.set_light_sensor(light_sensor))

    # Set thermal camera configuration if provided
    if "thermal_camera" in config:
        thermal_config = config["thermal_camera"]
        cg.add(var.set_thermal_refresh_rate(thermal_config[CONF_THERMAL_REFRESH_RATE]))
        cg.add(var.set_thermal_resolution(thermal_config[CONF_THERMAL_RESOLUTION]))
        cg.add(var.set_thermal_pattern(thermal_config[CONF_THERMAL_PATTERN]))
        cg.add(var.set_thermal_palette(thermal_config[CONF_THERMAL_PALETTE]))
        cg.add(var.set_thermal_single_frame(thermal_config[CONF_THERMAL_SINGLE_FRAME]))

    # Set ROI configuration if provided
    if CONF_ROI in config:
        roi_config = config[CONF_ROI]
        cg.add(var.set_roi_enabled(roi_config[CONF_ROI_ENABLED]))
        cg.add(var.set_roi_center_row(roi_config[CONF_ROI_CENTER_ROW]))
        cg.add(var.set_roi_center_col(roi_config[CONF_ROI_CENTER_COL]))
        cg.add(var.set_roi_size(roi_config[CONF_ROI_SIZE]))
