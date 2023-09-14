/*
 * Copyright (c) 2020 The ZMK Contributors
 *
 * SPDX-License-Identifier: MIT
 */

#include <zephyr/device.h>
#include <zephyr/init.h>

#include <zephyr/usb/usb_device.h>
#include <zephyr/usb/class/usb_hid.h>

#include <zmk/usb.h>
#include <zmk/hid.h>
#include <zmk/keymap.h>
#include <zmk/event_manager.h>

#if IS_ENABLED(CONFIG_BOARDPILOT)
#include <zmk/boardpilot/boardpilot.h>
#endif

LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

static const struct device *hid_dev;

#if IS_ENABLED(CONFIG_ZMK_BOARDPILOT)
static uint8_t bp_message_buffer[64];
static uint32_t bp_message_len = 0;
#endif

static K_SEM_DEFINE(hid_sem, 1, 1);

static void in_ready_cb(const struct device *dev) { k_sem_give(&hid_sem); }

#if IS_ENABLED(CONFIG_ZMK_BOARDPILOT)
static void out_ready_cb(const struct device *dev) {

    memset(bp_message_buffer, 0, 64);

    int err = hid_int_ep_read(dev, bp_message_buffer, 64, &bp_message_len);
    if (err < 0) {
        // Failed to read
        return;
    }

    err = zmk_boardpilot_control_parse(bp_message_buffer, bp_message_len);
    if (err < 0) {
        // Failed to parse
        return;
    }
}
#endif

static const struct hid_ops ops = {.int_in_ready = in_ready_cb,
#ifdef CONFIG_ENABLE_HID_INT_OUT_EP
                                   .int_out_ready = out_ready_cb
#endif
};

int zmk_usb_hid_send_report(const uint8_t *report, size_t len) {
    switch (zmk_usb_get_status()) {
    case USB_DC_SUSPEND:
        return usb_wakeup_request();
    case USB_DC_ERROR:
    case USB_DC_RESET:
    case USB_DC_DISCONNECTED:
    case USB_DC_UNKNOWN:
        return -ENODEV;
    default:
        k_sem_take(&hid_sem, K_MSEC(30));
        int err = hid_int_ep_write(hid_dev, report, len, NULL);

        if (err) {
            k_sem_give(&hid_sem);
        }

        return err;
    }
}

static int zmk_usb_hid_init(const struct device *_arg) {
    hid_dev = device_get_binding("HID_0");
    if (hid_dev == NULL) {
        LOG_ERR("Unable to locate HID device");
        return -EINVAL;
    }

    usb_hid_register_device(hid_dev, zmk_hid_report_desc, sizeof(zmk_hid_report_desc), &ops);
    usb_hid_init(hid_dev);

    return 0;
}

SYS_INIT(zmk_usb_hid_init, APPLICATION, CONFIG_APPLICATION_INIT_PRIORITY);
