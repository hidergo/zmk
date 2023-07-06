#define DT_DRV_COMPAT gooddisplay_il0323n

#include "il0323n.h"
#include <drivers/spi.h>
#include <string.h>

#include <logging/log.h>
LOG_MODULE_REGISTER(il0323n, CONFIG_DISPLAY_LOG_LEVEL);

#define IL0323_SPI_FREQ DT_INST_PROP(0, spi_max_frequency)
#define IL0323_BUS_NAME DT_INST_BUS_LABEL(0)
#define IL0323_DC_PIN DT_INST_GPIO_PIN(0, dc_gpios)
#define IL0323_DC_FLAGS DT_INST_GPIO_FLAGS(0, dc_gpios)
#define IL0323_DC_CNTRL DT_INST_GPIO_LABEL(0, dc_gpios)
#define IL0323_CS_PIN DT_INST_SPI_DEV_CS_GPIOS_PIN(0)
#define IL0323_CS_FLAGS DT_INST_SPI_DEV_CS_GPIOS_FLAGS(0)
#if DT_INST_SPI_DEV_HAS_CS_GPIOS(0)
#define IL0323_CS_CNTRL DT_INST_SPI_DEV_CS_GPIOS_LABEL(0)
#endif
#define IL0323_BUSY_PIN DT_INST_GPIO_PIN(0, busy_gpios)
#define IL0323_BUSY_CNTRL DT_INST_GPIO_LABEL(0, busy_gpios)
#define IL0323_BUSY_FLAGS DT_INST_GPIO_FLAGS(0, busy_gpios)
#define IL0323_RESET_PIN DT_INST_GPIO_PIN(0, reset_gpios)
#define IL0323_RESET_CNTRL DT_INST_GPIO_LABEL(0, reset_gpios)
#define IL0323_RESET_FLAGS DT_INST_GPIO_FLAGS(0, reset_gpios)

#define EPD_PANEL_WIDTH DT_INST_PROP(0, width)
#define EPD_PANEL_HEIGHT DT_INST_PROP(0, height)

// Clamp bound values
#define IL0323_CLAMP_BOUNDS

// How many times to refresh before doing a double reset
#define IL0323_EXTRA_REFRESH_INT 5

struct il0323_data {
    const struct device *reset;
    const struct device *dc;
    const struct device *busy;
    const struct device *spi_dev;
    struct spi_config spi_config;
#if defined(IL0323_CS_CNTRL)
    struct spi_cs_control cs_ctrl;
#endif
    uint8_t power_on;
    uint8_t partial_mode;
    uint8_t hibernating;
    // Partial refresh count for IL0323_EXTRA_REFRESH_INT
    uint16_t p_refresh_cnt;
};

// Pixel buffer
static uint8_t il0323_buffer[1280];
static uint8_t il0323_last_buffer[1280];

// Initialization programs
// 1B command, 1B data length, xB data

// Initialization program for il0323
const static uint8_t IL0323_INIT_PROG[] = {
    // CMD,  // LEN     // Data
    0xD2, 1, 0x3F,                   // ??
    0x00, 1, 0x4F,                   // PSR -> LUT from OTP
    0x01, 4, 0x03, 0x00, 0x2B, 0x2B, // Power settings
    0x06, 1, 0x3F,                   // Charge pump setting -> 50ms, strength 4, 8kHz
    0x2A, 2, 0x00, 0x00,             // LUT option
    0x30, 1, 0x13,                   // PLL
    0x50, 1, 0x57,                   // VCOM and Data interval settings
    0x60, 1, 0x22,                   // TCON
    0x61, 2, 0x50, 0x80,             // Resolution (80, 128)
    0x82, 1, 0x12,                   // VCOM DC -> -1V
    0xE3, 1, 0x33,                   // Power saving mode
};

// Partial initialization
const static uint8_t IL0323_PART_INIT_PROG[] = {
    // CMD,  // LEN     // Data
    0x00, 1, 0x6F, // PSR -> LUT from Registers
    0x30, 1, 0x05, // PLL -> 15Hz
    0x50, 1, 0xF2, // VCOM and Data interval
    0x82, 1, 0x00  // VCM DC setting
};

// LUT profiles
const static uint8_t IL0323_LUT_W_FULL[] = {
    0x60, 0x5A, 0x5A, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
};

const static uint8_t IL0323_LUT_B_FULL[] = {
    0x90, 0x5A, 0x5A, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
};

const static uint8_t IL0323_LUT_W_PARTIAL[] = {
    0x60, 0x01, 0x01, 0x00, 0x00, 0x01, 0x80, 0x0f, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
};

const static uint8_t IL0323_LUT_B_PARTIAL[] = {
    0x90, 0x01, 0x01, 0x00, 0x00, 0x01, 0x40, 0x0f, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
};

/**
 * @brief IL0323 Write register
 *
 * @param driver
 * @param cmd
 * @param data
 * @param len
 * @return int
 */
static int il0323_write_reg(struct il0323_data *driver, uint8_t reg, uint8_t *data, size_t len) {
    struct spi_buf buf = {.buf = &reg, .len = sizeof(reg)};
    struct spi_buf_set buf_set = {.buffers = &buf, .count = 1};

    gpio_pin_set(driver->dc, IL0323_DC_PIN, 1);
    if (spi_write(driver->spi_dev, &driver->spi_config, &buf_set)) {
        return -EIO;
    }

    if (data != NULL) {
        buf.buf = data;
        buf.len = len;
        gpio_pin_set(driver->dc, IL0323_DC_PIN, 0);
        if (spi_write(driver->spi_dev, &driver->spi_config, &buf_set)) {
            return -EIO;
        }
    }

    return 0;
}

/**
 * @brief Waits until il0323 is not busy
 * @todo Add a timeout
 *
 * @param driver
 */
static int il0323_busy_wait(struct il0323_data *driver) {
    int pin = gpio_pin_get(driver->busy, IL0323_BUSY_PIN);

    while (pin > 0) {
        // LOG_DBG("wait %u", pin);
        k_msleep(1);
        pin = gpio_pin_get(driver->busy, IL0323_BUSY_PIN);
    }

    return 0;
}

/**
 * @brief Resets the controller
 *
 * @param dev
 * @return int
 */
static int il0323_reset(struct il0323_data *driver) {

    gpio_pin_set(driver->reset, IL0323_RESET_PIN, 1);
    k_msleep(10);
    gpio_pin_set(driver->reset, IL0323_RESET_PIN, 0);
    k_msleep(10);
    il0323_busy_wait(driver);
    driver->hibernating = false;
    return 0;
}

/**
 * @brief Sets IL0323 power on
 *
 * @param driver
 * @param on on = 1, off = 0
 * @return int
 */
static int il0323_power(struct il0323_data *driver, uint8_t on) {

    // Prevent powering on/off multiple times
    if (on == driver->power_on)
        return 0;

    if (il0323_write_reg(driver, on ? 0x04 : 0x02, NULL, 0)) {
        return -EIO;
    }
    k_msleep(100);

    il0323_busy_wait(driver);

    driver->power_on = on;

    return 0;
}

int il0323_hibernate(const struct device *dev) {
    struct il0323_data *driver = dev->data;
    // Switch off power
    if (il0323_power(driver, false)) {
        return -EIO;
    }

    // Write deep sleep command
    uint8_t val = 0xA5;
    if (il0323_write_reg(driver, 0x07, &val, 1)) {
        return -EIO;
    }

    driver->hibernating = true;
}

/**
 * @brief Initializes driver to default values
 *
 * @param dev
 * @return int
 */
static int il0323_driver_init(const struct device *dev) {
    struct il0323_data *driver = dev->data;

    // Write registers
    int i = 0;
    while (i < sizeof(IL0323_INIT_PROG)) {

        if (il0323_write_reg(driver,
                             IL0323_INIT_PROG[i],      // Command
                             &IL0323_INIT_PROG[i + 2], // Data
                             IL0323_INIT_PROG[i + 1]   // Length
                             )) {
            return -EIO;
        }
        // Increment i, 1 + 1 + (length)
        i += 2 + IL0323_INIT_PROG[i + 1];
    }

    return 0;
}

/**
 * @brief Initializes driver to full mode
 *
 * @param dev
 * @return int
 */
static int il0323_driver_init_full(const struct device *dev) {
    int err = il0323_driver_init(dev);
    if (err) {
        return err;
    }

    struct il0323_data *driver = dev->data;

    // Write full LUT
    // White
    if (il0323_write_reg(driver, 0x23, IL0323_LUT_W_FULL, sizeof(IL0323_LUT_W_FULL))) {
        return -EIO;
    }

    // Black
    if (il0323_write_reg(driver, 0x24, IL0323_LUT_B_FULL, sizeof(IL0323_LUT_B_FULL))) {
        return -EIO;
    }

    driver->partial_mode = false;

    return 0;
}

/**
 * @brief Initializes driver to partial mode
 *
 * @param device
 * @return int
 */
static int il0323_driver_init_partial(const struct device *dev) {
    int err = il0323_driver_init(dev);
    if (err) {
        return err;
    }
    struct il0323_data *driver = dev->data;

    // Write partial registers
    int i = 0;
    while (i < sizeof(IL0323_PART_INIT_PROG)) {

        if (il0323_write_reg(driver,
                             IL0323_PART_INIT_PROG[i],      // Command
                             &IL0323_PART_INIT_PROG[i + 2], // Data
                             IL0323_PART_INIT_PROG[i + 1]   // Length
                             )) {
            return -EIO;
        }
        // Increment i, 1 + 1 + (length)
        i += 2 + IL0323_PART_INIT_PROG[i + 1];
    }

    // Write partial LUT
    // White
    if (il0323_write_reg(driver, 0x23, IL0323_LUT_W_PARTIAL, sizeof(IL0323_LUT_W_PARTIAL))) {
        return -EIO;
    }

    // Black
    if (il0323_write_reg(driver, 0x24, IL0323_LUT_B_PARTIAL, sizeof(IL0323_LUT_B_PARTIAL))) {
        return -EIO;
    }

    driver->partial_mode = true;

    return 0;
}

/**
 * @brief Sets the drawable area
 *
 * @param dev
 * @param x
 * @param y
 * @param w
 * @param h
 * @return int
 */
static int il0323_set_area(struct device *dev, uint16_t x, uint16_t y, uint16_t w, uint16_t h) {
    struct il0323_data *driver = dev->data;
    uint8_t bounds[] = {x & 0xFFF8,
                        (x + w - 1) | 0x0007, // byte boundary inclusive (last byte)
                        y, y + h - 1, 0x00};

    // Write boundaries
    if (il0323_write_reg(driver, 0x90, bounds, sizeof(bounds))) {
        return -EIO;
    }

    return 0;
}

static int il0323_init_buffer(struct device *dev) {
    struct il0323_data *driver = dev->data;

    // Reset buffer
    memset(il0323_buffer, 0xFF, sizeof(il0323_buffer));
    memset(il0323_last_buffer, 0xFF, sizeof(il0323_last_buffer));

    // Init old data
    if (il0323_write_reg(driver, 0x10, il0323_buffer, sizeof(il0323_buffer))) {
        return -EIO;
    }

    // Init new data
    if (il0323_write_reg(driver, 0x13, il0323_buffer, sizeof(il0323_buffer))) {
        return -EIO;
    }

    return 0;
}

int il0323_refresh(struct device *dev, int16_t x, int16_t y, int16_t w, int16_t h) {

    struct il0323_data *driver = dev->data;

    if (driver->hibernating) {
        // Reset
        il0323_reset(driver);
        k_msleep(10);
        il0323_driver_init_partial(dev);
    }

    // Init old data
    if (il0323_write_reg(driver, 0x10, il0323_last_buffer, sizeof(il0323_last_buffer))) {
        return -EIO;
    }

    // Init new data
    if (il0323_write_reg(driver, 0x13, il0323_buffer, sizeof(il0323_buffer))) {
        return -EIO;
    }

    memcpy(il0323_last_buffer, il0323_buffer, sizeof(il0323_last_buffer));

    il0323_power(dev->data, true);

    if (!driver->partial_mode) {
        // Full refresh
        if (il0323_write_reg(driver, 0x12, NULL, 0)) {
            return -EIO;
        }

        // Wait until refreshed
        il0323_busy_wait(driver);
        return 0;
    }

    int16_t w1 = x < 0 ? w + x : w;                                                 // reduce
    int16_t h1 = y < 0 ? h + y : h;                                                 // reduce
    int16_t x1 = x < 0 ? 0 : x;                                                     // limit
    int16_t y1 = y < 0 ? 0 : y;                                                     // limit
    w1 = x1 + w1 < (int16_t)EPD_PANEL_WIDTH ? w1 : (int16_t)EPD_PANEL_WIDTH - x1;   // limit
    h1 = y1 + h1 < (int16_t)EPD_PANEL_HEIGHT ? h1 : (int16_t)EPD_PANEL_HEIGHT - y1; // limit
    if ((w1 <= 0) || (h1 <= 0))
        return;
    // make x1, w1 multiple of 8
    w1 += x1 % 8;
    if (w1 % 8 > 0)
        w1 += 8 - w1 % 8;
    x1 -= x1 % 8;

    il0323_busy_wait(driver);

    // Enter partial mode
    if (il0323_write_reg(driver, 0x91, NULL, 0)) {
        return -EIO;
    }

    // Set area
    if (il0323_set_area(dev, x1, y1, w1, h1)) {
        return -EIO;
    }

    il0323_busy_wait(driver);

    // Update part
    if (il0323_write_reg(driver, 0x12, NULL, 0)) {
        return -EIO;
    }

    // Wait until refreshed
    il0323_busy_wait(driver);

    // Exit partial mode
    if (il0323_write_reg(driver, 0x92, NULL, 0)) {
        return -EIO;
    }
    // Check for double refresh
    if (driver->p_refresh_cnt >= (uint16_t)IL0323_EXTRA_REFRESH_INT) {
        // Reset last buffer to white, to trigger refresh for black pixels
        memset(il0323_last_buffer, 0xFF, sizeof(il0323_last_buffer));
        driver->p_refresh_cnt = 0;
        // Call refresh
        return il0323_refresh(dev, x, y, w, h);
    }
    driver->p_refresh_cnt++;

    return 0;
}

void il0323_clear_area(const struct device *dev, uint8_t x, uint8_t y, uint8_t w, uint8_t h) {
    struct il0323_data *driver = dev->data;

#ifdef IL0323_CLAMP_BOUNDS
    // Clamp x y
    x = (x <= EPD_PANEL_WIDTH) ? x : EPD_PANEL_WIDTH;
    y = (y <= EPD_PANEL_HEIGHT) ? y : EPD_PANEL_HEIGHT;
    // Clamp w h
    w = (x + w <= EPD_PANEL_WIDTH) ? w : EPD_PANEL_WIDTH;
    h = (y + h <= EPD_PANEL_HEIGHT) ? h : EPD_PANEL_HEIGHT;
#endif

    for (int ly = y; ly < y + h; ly++) {
        for (int lx = x; lx < x + w; lx++) {
            il0323_buffer[ly * (EPD_PANEL_WIDTH / 8) + lx / 8] |= 1 << (7 - (lx % 8));
        }
    }

    return;
}

void il0323_set_pixel(const struct device *dev, uint16_t x, uint16_t y) {
    struct il0323_data *driver = dev->data;

#ifdef IL0323_CLAMP_BOUNDS
    // Don't draw outside bounds
    if (x >= EPD_PANEL_WIDTH || y >= EPD_PANEL_HEIGHT)
        return;
#endif

    il0323_buffer[y * (EPD_PANEL_WIDTH / 8) + x / 8] &= ~(1 << (7 - (x % 8)));

    return;
}

void il0323_clear_pixel(const struct device *dev, uint8_t x, uint8_t y) {
    struct il0323_data *driver = dev->data;

#ifdef IL0323_CLAMP_BOUNDS
    // Don't draw outside bounds
    if (x >= EPD_PANEL_WIDTH || y >= EPD_PANEL_HEIGHT)
        return;
#endif
    il0323_buffer[y * (EPD_PANEL_WIDTH / 8) + x / 8] |= 1 << (7 - (x % 8));

    return;
}

void il0323_h_line(const struct device *dev, uint8_t x, uint8_t y, uint8_t len) {
    struct il0323_data *driver = dev->data;

#ifdef IL0323_CLAMP_BOUNDS
    if ((uint16_t)len + (uint16_t)x >= EPD_PANEL_WIDTH) {
        len = EPD_PANEL_WIDTH - x - 1;
    }
    // Don't draw outside bounds
    if (x >= EPD_PANEL_WIDTH || y >= EPD_PANEL_HEIGHT)
        return;
#endif
    for (int i = x; i < x + len; i++) {
        il0323_buffer[y * (EPD_PANEL_WIDTH / 8) + i / 8] &= ~(1 << (7 - (i % 8)));
    }

    return;
}

void il0323_v_line(const struct device *dev, uint8_t x, uint8_t y, uint8_t len) {
    struct il0323_data *driver = dev->data;

#ifdef IL0323_CLAMP_BOUNDS
    if ((uint16_t)len + (uint16_t)y >= EPD_PANEL_HEIGHT) {
        len = EPD_PANEL_HEIGHT - y - 1;
    }
    // Don't draw outside bounds
    if (x >= EPD_PANEL_WIDTH || y >= EPD_PANEL_HEIGHT)
        return;
#endif
    for (int i = y; i < y + len; i++) {
        il0323_buffer[i * (EPD_PANEL_WIDTH / 8) + x / 8] &= ~(1 << (7 - (x % 8)));
    }

    return;
}

int init_err = 0;

int il0323_init_regs(const struct device *dev) {
    struct il0323_data *driver = dev->data;

    // Reset device
    init_err |= il0323_reset(driver);

    // Full mode for refresh
    init_err |= il0323_driver_init_full(dev);
    // il0323_busy_wait(driver);
    //  Power ON
    init_err |= il0323_power(driver, true);

    // Initialize buffer
    init_err |= il0323_init_buffer(dev);

    // Refresh
    // init_err |= il0323_refresh(dev, 0, 0, 80, 128);

    // Initialize partial mode
    // init_err |= il0323_driver_init_partial(dev);

    // il0323_busy_wait(driver);

    // il0323_hibernate(dev);

    return 0;
}

/**
 * @brief IL0323 Initialization
 *
 * @param device
 * @return int
 */
static int il0323_init(const struct device *dev) {
    struct il0323_data *driver = dev->data;

    driver->spi_dev = device_get_binding(IL0323_BUS_NAME);
    if (driver->spi_dev == NULL) {
        LOG_ERR("Could not get SPI device for IL0323");
        return -EIO;
    }

    driver->spi_config.frequency = IL0323_SPI_FREQ;
    driver->spi_config.operation = SPI_OP_MODE_MASTER | SPI_WORD_SET(8);
    driver->spi_config.slave = DT_INST_REG_ADDR(0);
    driver->spi_config.cs = NULL;

    driver->power_on = false;
    driver->partial_mode = false;

    driver->reset = device_get_binding(IL0323_RESET_CNTRL);
    if (driver->reset == NULL) {
        LOG_ERR("Could not get GPIO port for IL0323 reset");
        return -EIO;
    }

    gpio_pin_configure(driver->reset, IL0323_RESET_PIN, GPIO_OUTPUT_INACTIVE | IL0323_RESET_FLAGS);

    driver->dc = device_get_binding(IL0323_DC_CNTRL);
    if (driver->dc == NULL) {
        LOG_ERR("Could not get GPIO port for IL0323 DC signal");
        return -EIO;
    }

    gpio_pin_configure(driver->dc, IL0323_DC_PIN, GPIO_OUTPUT_INACTIVE | IL0323_DC_FLAGS);

    driver->busy = device_get_binding(IL0323_BUSY_CNTRL);
    if (driver->busy == NULL) {
        LOG_ERR("Could not get GPIO port for IL0323 busy signal");
        return -EIO;
    }

    gpio_pin_configure(driver->busy, IL0323_BUSY_PIN, GPIO_INPUT | IL0323_BUSY_FLAGS);

#if defined(IL0323_CS_CNTRL)
    driver->cs_ctrl.gpio_dev = device_get_binding(IL0323_CS_CNTRL);
    if (!driver->cs_ctrl.gpio_dev) {
        LOG_ERR("Unable to get SPI GPIO CS device");
        return -EIO;
    }

    driver->cs_ctrl.gpio_pin = IL0323_CS_PIN;
    driver->cs_ctrl.gpio_dt_flags = IL0323_CS_FLAGS;
    driver->cs_ctrl.delay = 0U;
    driver->spi_config.cs = &driver->cs_ctrl;
#endif

    return 0;
}

static struct il0323_data il0323_driver;

struct il0323_api {};
static struct il0323_api il0323_driver_api = {

};

DEVICE_DT_INST_DEFINE(0, il0323_init, NULL, &il0323_driver, NULL, POST_KERNEL,
                      CONFIG_APPLICATION_INIT_PRIORITY, &il0323_driver_api);