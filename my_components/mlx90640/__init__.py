import esphome.codegen as cg
from esphome.components import sensor
import esphome.config_validation as cv
from esphome.const import CONF_ID

DEPENDENCIES = ["esp32", "web_server_base"]

CONF_REFRESH_RATE = "refresh_rate"
CONF_RESOLUTION = "resolution"
CONF_PATTERN = "pattern"
CONF_SINGLE_FRAME = "single_frame"
CONF_UPDATE_INTERVAL = "update_interval"
CONF_THERMAL_PALETTE = "thermal_palette"
CONF_ROI = "roi"
CONF_ROI_ENABLED = "enabled"
CONF_ROI_CENTER_ROW = "center_row"
CONF_ROI_CENTER_COL = "center_col"
CONF_ROI_SIZE = "size"
CONF_TEMPERATURE_MIN = "temperature_min"
CONF_TEMPERATURE_MAX = "temperature_max"
CONF_TEMPERATURE_AVG = "temperature_avg"
CONF_ROI_MIN = "roi_min"
CONF_ROI_MAX = "roi_max"
CONF_ROI_AVG = "roi_avg"
CONF_WEB_SERVER = "web_server"
CONF_WEB_ENABLE = "enable"
CONF_WEB_PATH = "path"
CONF_WEB_WIDTH = "width"
CONF_WEB_HEIGHT = "height"
CONF_WEB_QUALITY = "quality"

# Component configuration documentation:
#
# REFRESH RATES:
# - Lower rates (0.5Hz-8Hz): Better for static scenes, less I2C bus load
# - Higher rates (16Hz-64Hz): Better for motion detection, requires more I2C bandwidth
#
# PATTERNS:
# - "chess": Chess pattern readout (default) - vendor recommended, better for even temperature distribution
# - "interleaved": Interleaved pattern readout - different readout pattern
#
# SINGLE FRAME:
# - false (default): Read both subpages for better image quality
# - true: Read only one frame to reduce motion artifacts (checkerboard pattern)
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

mlx90640_ns = cg.esphome_ns.namespace("mlx90640")
MLX90640Component = mlx90640_ns.class_("MLX90640Component", cg.Component)
ROIConfig = mlx90640_ns.struct("ROIConfig")

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(MLX90640Component),
        cv.Optional(CONF_REFRESH_RATE, default="16Hz"): cv.one_of(
            "0.5Hz", "1Hz", "2Hz", "4Hz", "8Hz", "16Hz", "32Hz", "64Hz"
        ),
        cv.Optional(CONF_RESOLUTION, default="18-bit"): cv.one_of(
            "16-bit", "17-bit", "18-bit", "19-bit"
        ),
        cv.Optional(CONF_PATTERN, default="chess"): cv.one_of("chess", "interleaved"),
        cv.Optional(CONF_SINGLE_FRAME, default=False): cv.boolean,
        cv.Optional(CONF_UPDATE_INTERVAL, default=20000): cv.positive_int,
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
        cv.Optional(CONF_TEMPERATURE_MIN): cv.use_id(sensor.Sensor),
        cv.Optional(CONF_TEMPERATURE_MAX): cv.use_id(sensor.Sensor),
        cv.Optional(CONF_TEMPERATURE_AVG): cv.use_id(sensor.Sensor),
        cv.Optional(CONF_ROI_MIN): cv.use_id(sensor.Sensor),
        cv.Optional(CONF_ROI_MAX): cv.use_id(sensor.Sensor),
        cv.Optional(CONF_ROI_AVG): cv.use_id(sensor.Sensor),
        cv.Optional(CONF_WEB_SERVER): cv.Schema(
            {
                cv.Optional(CONF_WEB_ENABLE, default=False): cv.boolean,
                cv.Optional(CONF_WEB_PATH, default="/thermal.jpg"): cv.string,
                cv.Optional(CONF_WEB_WIDTH, default=320): cv.int_range(
                    min=64, max=1024
                ),
                cv.Optional(CONF_WEB_HEIGHT, default=240): cv.int_range(
                    min=48, max=768
                ),
                cv.Optional(CONF_WEB_QUALITY, default=85): cv.int_range(
                    min=10, max=100
                ),
            }
        ),
    }
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)

    # Add JPEGENC library for JPEG generation
    cg.add_platformio_option("lib_deps", ["https://github.com/bitbank2/JPEGENC.git"])

    # Set basic configuration
    cg.add(var.set_refresh_rate(config[CONF_REFRESH_RATE]))
    cg.add(var.set_resolution(config[CONF_RESOLUTION]))
    cg.add(var.set_pattern(config[CONF_PATTERN]))
    cg.add(var.set_single_frame(config[CONF_SINGLE_FRAME]))
    cg.add(var.set_update_interval(config[CONF_UPDATE_INTERVAL]))
    cg.add(var.set_thermal_palette(config[CONF_THERMAL_PALETTE]))

    # Set ROI configuration if provided
    if CONF_ROI in config:
        roi_config = config[CONF_ROI]
        # Create ROI config struct
        roi_struct = cg.StructInitializer(
            ROIConfig,
            ("enabled", roi_config[CONF_ROI_ENABLED]),
            ("center_row", roi_config[CONF_ROI_CENTER_ROW]),
            ("center_col", roi_config[CONF_ROI_CENTER_COL]),
            ("size", roi_config[CONF_ROI_SIZE]),
        )
        cg.add(var.set_roi_config(roi_struct))

    # Wire up temperature sensors if provided
    if CONF_TEMPERATURE_MIN in config:
        sensor_var = await cg.get_variable(config[CONF_TEMPERATURE_MIN])
        cg.add(var.set_temperature_min_sensor(sensor_var))

    if CONF_TEMPERATURE_MAX in config:
        sensor_var = await cg.get_variable(config[CONF_TEMPERATURE_MAX])
        cg.add(var.set_temperature_max_sensor(sensor_var))

    if CONF_TEMPERATURE_AVG in config:
        sensor_var = await cg.get_variable(config[CONF_TEMPERATURE_AVG])
        cg.add(var.set_temperature_avg_sensor(sensor_var))

    if CONF_ROI_MIN in config:
        sensor_var = await cg.get_variable(config[CONF_ROI_MIN])
        cg.add(var.set_roi_min_sensor(sensor_var))

    if CONF_ROI_MAX in config:
        sensor_var = await cg.get_variable(config[CONF_ROI_MAX])
        cg.add(var.set_roi_max_sensor(sensor_var))

    if CONF_ROI_AVG in config:
        sensor_var = await cg.get_variable(config[CONF_ROI_AVG])
        cg.add(var.set_roi_avg_sensor(sensor_var))

    # Configure web server if enabled
    if CONF_WEB_SERVER in config:
        web_config = config[CONF_WEB_SERVER]
        if web_config[CONF_WEB_ENABLE]:
            cg.add(var.set_web_server_enabled(True))
            cg.add(var.set_web_server_path(web_config[CONF_WEB_PATH]))
            cg.add(var.set_web_server_width(web_config[CONF_WEB_WIDTH]))
            cg.add(var.set_web_server_height(web_config[CONF_WEB_HEIGHT]))
            cg.add(var.set_web_server_quality(web_config[CONF_WEB_QUALITY]))
