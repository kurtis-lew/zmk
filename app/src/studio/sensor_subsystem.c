/*
 * Copyright (c) 2025 The ZMK Contributors
 *
 * SPDX-License-Identifier: MIT
 */

#include <drivers/behavior.h>
#include <zephyr/logging/log.h>
LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

#include <pb_encode.h>
#include <zmk/keymap.h>
#include <zmk/behavior.h>
#include <zmk/sensors.h>
#include <zmk/studio/rpc.h>

ZMK_RPC_SUBSYSTEM(sensors)

#define SENSORS_RESPONSE(type, ...) ZMK_RPC_RESPONSE(sensors, type, __VA_ARGS__)
#define SENSORS_NOTIFICATION(type, ...) ZMK_RPC_NOTIFICATION(keymap, type, __VA_ARGS__)

zmk_studio_Response get_sensor_binding(const zmk_studio_Request *req) {
    LOG_DBG("");
    const zmk_sensors_GetSensorBindingRequest *get_req =
        &req->subsystem.sensors.request_type.get_sensor_binding;

    const struct zmk_behavior_binding *binding =
        zmk_keymap_get_sensor_binding_at_idx(get_req->layer_id, get_req->sensor_index);

    zmk_sensors_SensorBinding resp = zmk_sensors_SensorBinding_init_zero;
    resp.computed_behavior_id = zmk_behavior_get_local_id(binding->behavior_dev);
    resp.behavior_id = binding->local_id;
    resp.param1 = binding->param1;
    resp.param2 = binding->param2;

    return SENSORS_RESPONSE(get_sensor_binding, resp);
}

zmk_studio_Response set_sensor_binding(const zmk_studio_Request *req) {
    LOG_DBG("");
    const zmk_sensors_SetSensorBindingRequest *set_req =
        &req->subsystem.sensors.request_type.set_sensor_binding;

    zmk_behavior_local_id_t bid = set_req->binding.behavior_id;

    const char *behavior_name = zmk_behavior_find_behavior_name_from_local_id(bid);

    if (!behavior_name) {
        return SENSORS_RESPONSE(
            set_sensor_binding,
            zmk_sensors_SetSensorBindingResponse_SET_SENSOR_BINDING_RESP_INVALID_BEHAVIOR);
    }

    struct zmk_behavior_binding binding = (struct zmk_behavior_binding){
        .behavior_dev = behavior_name,
        .param1 = set_req->binding.param1,
        .param2 = set_req->binding.param2,
    };

    int ret = zmk_behavior_validate_binding(&binding);
    if (ret < 0) {
        return SENSORS_RESPONSE(
            set_sensor_binding,
            zmk_sensors_SetSensorBindingResponse_SET_SENSOR_BINDING_RESP_INVALID_PARAMETERS);
    }

    ret = zmk_keymap_set_sensor_binding_at_idx(set_req->layer_id, set_req->sensor_index, binding);

    if (ret < 0) {
        LOG_WRN("Setting the binding failed with %d", ret);
        switch (ret) {
        case -EINVAL:
            return SENSORS_RESPONSE(
                set_sensor_binding,
                zmk_sensors_SetSensorBindingResponse_SET_SENSOR_BINDING_RESP_INVALID_INDEX);
        default:
            return ZMK_RPC_SIMPLE_ERR(GENERIC);
        }
    }

    raise_zmk_studio_rpc_notification((struct zmk_studio_rpc_notification){
        .notification = SENSORS_NOTIFICATION(unsaved_changes_status_changed, true)});

    return SENSORS_RESPONSE(set_sensor_binding,
                            zmk_sensors_SetSensorBindingResponse_SET_SENSOR_BINDING_RESP_OK);
}

zmk_studio_Response get_sensor_config(const zmk_studio_Request *req) {
    LOG_DBG("");
    const uint8_t sensor_index = (uint8_t)req->subsystem.sensors.request_type.get_sensor_config;

    const struct zmk_sensor_config *config = zmk_sensors_get_config_at_index(sensor_index);
    if (config == NULL) {
        return ZMK_RPC_SIMPLE_ERR(GENERIC);
    }

    zmk_sensors_SensorConfig resp = zmk_sensors_SensorConfig_init_zero;
    resp.triggers_per_rotation = config->triggers_per_rotation;

    return SENSORS_RESPONSE(get_sensor_config, resp);
}

zmk_studio_Response set_sensor_config(const zmk_studio_Request *req) {
    LOG_DBG("");
    const zmk_sensors_SetSensorConfigRequest *set_req =
        &req->subsystem.sensors.request_type.set_sensor_config;

    uint8_t sensor_index = (uint8_t)set_req->sensor_index;
    uint16_t triggers_per_rotation = (uint16_t)set_req->triggers_per_rotation;

    zmk_sensors_set_config_at_index(sensor_index, triggers_per_rotation);

    raise_zmk_studio_rpc_notification((struct zmk_studio_rpc_notification){
        .notification = SENSORS_NOTIFICATION(unsaved_changes_status_changed, true)});

    return SENSORS_RESPONSE(set_sensor_binding,
                            zmk_sensors_SetSensorConfigResponse_SET_SENSOR_CONFIG_RESP_OK);
}

ZMK_RPC_SUBSYSTEM_HANDLER(sensors, get_sensor_binding, ZMK_STUDIO_RPC_HANDLER_SECURED);
ZMK_RPC_SUBSYSTEM_HANDLER(sensors, set_sensor_binding, ZMK_STUDIO_RPC_HANDLER_SECURED);
ZMK_RPC_SUBSYSTEM_HANDLER(sensors, get_sensor_config, ZMK_STUDIO_RPC_HANDLER_SECURED);
ZMK_RPC_SUBSYSTEM_HANDLER(sensors, set_sensor_config, ZMK_STUDIO_RPC_HANDLER_SECURED);

static int sensors_event_mapper(const zmk_event_t *eh, zmk_studio_Notification *n) { return 0; }

ZMK_RPC_EVENT_MAPPER(sensors, sensors_event_mapper);
