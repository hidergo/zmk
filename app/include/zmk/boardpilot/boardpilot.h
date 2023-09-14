#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zmk/behavior.h>

// Flag if this field should be saved to NVS
#define ZMK_BOARDPILOT_FIELD_FLAG_SAVEABLE BIT(0)
// Flag if this field has been read from NVS
#define ZMK_BOARDPILOT_FIELD_FLAG_READ BIT(1)
// Flag if this field has been written to NVS
#define ZMK_BOARDPILOT_FIELD_FLAG_WRITTEN BIT(2)

/**
 * @brief BoardPilot configuration keys, casted as uint16_t
 *
 */
enum zmk_boardpilot_field_key {
    // Invalid key
    ZMK_CONFIG_KEY_INVALID = 0x0000,
    // --------------------------------------------------------------
    // 0x0001 - 0x3FFF: (Recommended) saveable fields
    // Fields that should be saved to NVS, such as keymap or mouse sensitivity
    // --------------------------------------------------------------

    // 0x0001 - 0x0009: Device information fields
    // Device info (struct zmk_config_device_info)
    ZMK_CONFIG_KEY_DEVICE_INFO = 0x0001,
    // 0x000A - 0x001F: Device configuration
    // Sleep timeout (u16) (0 = never sleep)
    ZMK_CONFIG_KEY_SLEEP_TIMEOUT = 0x000A,
    // Peripheral sleep timeout (u16) (0 = never sleep)
    ZMK_CONFIG_KEY_PERIPHERAL_SLEEP_TIMEOUT = 0x000B,

    // 0x0020 - 0x003F: Keyboard configurations
    // Keymap
    ZMK_CONFIG_KEY_KEYMAP = 0x0020,

    // 0x0040 - 0x005F: Mouse/trackpad configurations
    // Mouse sensitivity (u8)
    ZMK_CONFIG_KEY_MOUSE_SENSITIVITY = 0x0040,
    // Mouse Y scroll sensitivity (u8)
    ZMK_CONFIG_KEY_SCROLL_SENSITIVITY = 0x0041,
    // Mouse X pan sensitivity (u8)
    ZMK_CONFIG_KEY_PAN_SENSITIVITY = 0x0042,
    // Mouse scroll direction (u8)
    ZMK_CONFIG_KEY_SCROLL_DIRECTION = 0x0043,
    // Touchpad click type (u8) (0 = normal, 1 = left click on left side, right click on right side)
    ZMK_CONFIG_KEY_TP_CLICK_TYPE = 0x0044,

    // 0x0060 - 0x007F: Display configurations
    ZMK_CONFIG_KEY_DISPLAY_CODE = 0x0060,

    // --------------------------------------------------------------
    // 0x4000 - 0x7FFF: (Recommended) Non-saved fields
    // Fields that do not require saving to NVS, such as time or date
    // --------------------------------------------------------------

    // (int32_t[2]) [0] Unix timestamp of time, [1] timezone in seconds
    ZMK_CONFIG_KEY_DATETIME = 0x4000,

    // --------------------------------------------------------------
    // 0x6000 - 0x8000: Custom fields
    // Fields that should be used if custom fields are needed
    // --------------------------------------------------------------

};

// BoardPilot configuration field
struct zmk_boardpilot_field {
    // Key identifier (enum zmk_boardpilot_field_key)
    uint16_t key;
    // Bit mask of field flags, see ZMK_BOARDPILOT_FIELD_FLAG_* defs
    uint8_t flags;
    // Mutex lock, if needed for thread safety
    struct k_mutex mutex;
    // Device handle
    struct device *device;
    // Callback to be triggered when data is updated with BoardPilot
    void (*on_update)(struct zmk_boardpilot_field *field);
    // Allocated size of the field in bytes
    uint16_t size;
    // Local data, should be initialized to NULL
    void *data;
};

// Single key rebinding
struct __attribute__((packed)) zmk_boardpilot_binding {
    // Key position
    // Layer is the least significant 4 bits (key & 0x0F)
    // Key position is the most significant 12 bits (key >> 4)
    uint16_t key;
    // Device/behavior.
    // Corresponds to a device in ZMK_BOARDPILOT_DEVICE_ID
    uint8_t device;
    // Binding parameter 1
    uint32_t param1;
    // Binding parameter 2
    uint32_t param2;
};

/**
 * @brief Binds a variable to be saved or altered by BoardPilot
 *
 * @param key Key of the field which will be bound
 * @param data Pointer to the data which will be saved. This should not be dynamically allocated
 * @param size Size of the value in "data"
 * @param saveable true if this value can be saved to NVS
 * @param update_callback Callback which is triggered when the value is externally changed. Can be
 * NULL
 * @param device If a reference to a device is needed, this can be passed. Otherwise NULL
 *
 * @return Bound field. NULL if failed
 */
struct zmk_boardpilot_field *
zmk_boardpilot_bind(enum zmk_boardpilot_field_key key, void *data, uint16_t size, bool saveable,
                    void (*update_callback)(struct zmk_boardpilot_field *), struct device *device);

/**
 * @brief Gets a field by the key. NOTE: does NOT read from NVS
 *
 * @param key Key of the field to get
 * @return Field. NULL if not found
 */
struct zmk_boardpilot_field *zmk_boardpilot_get(enum zmk_boardpilot_field_key key);

/**
 * @brief Read configuration field. This reads the value from the NVS to the field.
 *
 * @param key Key of the field to read
 * @return 0 on success, negative on fail
 */
int zmk_boardpilot_read(enum zmk_boardpilot_field_key key);

/**
 * @brief Writes to NVS
 *
 * @param key Key of the field to write
 *
 * @return 0 on success, negative on fail
 */
int zmk_boardpilot_write(enum zmk_boardpilot_field_key key);

/**
 * @brief Transform config keymap item to zmk_behavior_binding. Sets binding
 *
 * @param binding This field is altered if corresponding device is found. Should not be NULL!
 * @param item Config item
 * @return 0 on success, -1 on error
 */
int zmk_boardpilot_keymap_conf_to_binding(struct zmk_behavior_binding *binding,
                                          struct zmk_boardpilot_binding *item);

/**
 * @brief Transform zmk_behavior_binding to zmk_boardpilot_binding
 *
 * @param binding
 * @param item
 * @return 0 on success, -1 on error
 */
int zmk_boardpilot_keymap_binding_to_conf(struct zmk_behavior_binding *binding,
                                          struct zmk_boardpilot_binding *item, uint8_t layer,
                                          uint16_t key);
