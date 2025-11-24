/*
 * Copyright (c) 2025 The ZMK Contributors
 *
 * SPDX-License-Identifier: MIT
 */

#define DT_DRV_COMPAT zmk_input_qdec

#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/input/input.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);
struct input_qdec_config {
    const struct gpio_dt_spec a;
    const struct gpio_dt_spec b;

    const uint8_t steps_per_period;
};

struct input_qdec_data {
    const struct device *dev;
    uint8_t ab_state;
    int8_t pulses;
    int8_t ticks;
    int8_t delta;
    struct k_work event_work;
    struct k_work_delayable idle_work;
    struct gpio_callback gpio_a_cb;
    struct gpio_callback gpio_b_cb;
};

static int input_qdec_get_ab_state(const struct device *dev) {
    const struct input_qdec_config *drv_cfg = dev->config;
    return (gpio_pin_get_dt(&drv_cfg->a) << 1) | gpio_pin_get_dt(&drv_cfg->b);
}

static int input_qdec_sample_fetch(const struct device *dev, enum sensor_channel chan) {
    struct input_qdec_data *drv_data = dev->data;
    const struct input_qdec_config *drv_cfg = dev->config;
    uint8_t val;
    int8_t delta;

    __ASSERT_NO_MSG(chan == SENSOR_CHAN_ALL || chan == SENSOR_CHAN_ROTATION);

    val = input_qdec_get_ab_state(dev);

    LOG_DBG("prev: %d, new: %d", drv_data->ab_state, val);

    switch (val | (drv_data->ab_state << 2)) {
    case 0b0010:
    case 0b0100:
    case 0b1101:
    case 0b1011:
        delta = -1;
        break;
    case 0b0001:
    case 0b0111:
    case 0b1110:
    case 0b1000:
        delta = 1;
        break;
    default:
        delta = 0;
        break;
    }

    LOG_DBG("Delta: %d", delta);

    drv_data->pulses += delta;
    drv_data->ab_state = val;

    // TODO: Temporary code for backwards compatibility to support
    // the sensor channel rotation reporting *ticks* instead of delta of degrees.
    // REMOVE ME
    if (drv_cfg->steps == 0) {
        drv_data->ticks = drv_data->pulses / drv_cfg->resolution;
        drv_data->delta = delta;
        drv_data->pulses %= drv_cfg->resolution;
    }

    return 0;
}

static int input_qdec_channel_get(const struct device *dev, enum sensor_channel chan,
                                  struct sensor_value *val) {
    struct input_qdec_data *drv_data = dev->data;
    const struct input_qdec_config *drv_cfg = dev->config;
    int32_t pulses = drv_data->pulses;

    if (chan != SENSOR_CHAN_ROTATION) {
        return -ENOTSUP;
    }

    drv_data->pulses = 0;

    if (drv_cfg->steps > 0) {
        val->val1 = (pulses * FULL_ROTATION) / drv_cfg->steps;
        val->val2 = (pulses * FULL_ROTATION) % drv_cfg->steps;
        if (val->val2 != 0) {
            val->val2 *= 1000000;
            val->val2 /= drv_cfg->steps;
        }
    } else {
        val->val1 = drv_data->ticks;
        val->val2 = drv_data->delta;
    }

    return 0;
}

static const struct sensor_driver_api input_qdec_driver_api = {
#ifdef CONFIG_EC11_TRIGGER
    .trigger_set = input_qdec_trigger_set,
#endif
    .sample_fetch = input_qdec_sample_fetch,
    .channel_get = input_qdec_channel_get,
};

int input_qdec_init(const struct device *dev) {
    struct input_qdec_data *drv_data = dev->data;
    const struct input_qdec_config *drv_cfg = dev->config;

    data->dev = dev;

    LOG_DBG("A: %s %d B: %s %d", drv_cfg->a.port->name, drv_cfg->a.pin,
            drv_cfg->b.port->name, drv_cfg->b.pin;
    
    k_work_init(&data->event_work, gpio_qdec_event_worker);
	k_work_init_delayable(&data->idle_work, gpio_qdec_idle_worker);

	k_timer_init(&data->sample_timer, gpio_qdec_sample_timer_timeout, NULL);
	k_timer_user_data_set(&data->sample_timer, (void *)dev);

    gpio_init_callback(&data->gpio_cb, gpio_qdec_cb,
			   BIT(cfg->gpio[0].pin) | BIT(cfg->gpio[1].pin));

    gpio_init_callback(&data->gpio_cb, gpio_qdec_cb,
			   BIT(cfg->gpio[0].pin) | BIT(cfg->gpio[1].pin));

    if (!device_is_ready(drv_cfg->a.port)) {
        LOG_ERR("A GPIO device is not ready");
        return -EINVAL;
    }

    if (!device_is_ready(drv_cfg->b.port)) {
        LOG_ERR("B GPIO device is not ready");
        return -EINVAL;
    }

    if (gpio_pin_configure_dt(&drv_cfg->a, GPIO_INPUT)) {
        LOG_DBG("Failed to configure A pin");
        return -EIO;
    }

    if (gpio_pin_configure_dt(&drv_cfg->b, GPIO_INPUT)) {
        LOG_DBG("Failed to configure B pin");
        return -EIO;
    }

    drv_data->ab_state = input_qdec_get_ab_state(dev);

    return 0;
}

#define INPUT_QDEC_INST(n)                                                                         \
    static struct input_qdec_data input_qdec_data_##n = {};                                        \
    static const struct input_qdec_config input_qdec_cfg_##n = {                                   \
        .a = GPIO_DT_SPEC_INST_GET(n, a_gpios),                                                    \
        .b = GPIO_DT_SPEC_INST_GET(n, b_gpios),                                                    \
        .steps_per_period = DT_INST_PROP_(n, steps_per_period),                                    \
    };                                                                                             \
    DEVICE_DT_INST_DEFINE(n, input_qdec_init, NULL, &input_qdec_data_##n, &input_qdec_cfg_##n,     \
                          POST_KERNEL, CONFIG_INPUT_INIT_PRIORITY, NULL);

DT_INST_FOREACH_STATUS_OKAY(INPUT_MOCK_INST)
