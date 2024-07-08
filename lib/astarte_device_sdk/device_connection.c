/*
 * (C) Copyright 2024, SECO Mind Srl
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "device_connection.h"

#if defined(CONFIG_ASTARTE_DEVICE_SDK_PERMANENT_STORAGE)
#include "device_caching.h"
#endif

#include "log.h"
ASTARTE_LOG_MODULE_REGISTER(
    device_connection, CONFIG_ASTARTE_DEVICE_SDK_DEVICE_CONNECTION_LOG_LEVEL);

/************************************************
 *         Static functions declaration         *
 ***********************************************/

/**
 * @brief Setup all the MQTT subscriptions for the device.
 *
 * @param[in] device Handle to the device instance.
 */
static void setup_subscriptions(astarte_device_handle_t device);
/**
 * @brief Send the introspection for the device.
 *
 * @param[in] device Handle to the device instance.
 */
static void send_introspection(astarte_device_handle_t device);
/**
 * @brief Send the emptycache message to Astarte.
 *
 * @param[in] device Handle to the device instance.
 */
static void send_emptycache(astarte_device_handle_t device);
#if defined(CONFIG_ASTARTE_DEVICE_SDK_PERMANENT_STORAGE)
/**
 * @brief Send the device owned properties to Astarte.
 *
 * @param[in] device Handle to the device instance.
 */
static void send_device_owned_properties(astarte_device_handle_t device);
/**
 * @brief Check if a property is in the device introspection.
 *
 * @note If the property is not found in the introspection it's deleted from the cache.
 *
 * @param[in] device Handle to the device instance.
 * @param[in] interface_name Name of the interface for the property as retreived from cache.
 * @param[in] path Path for the property as retreived from cache.
 * @param[in] major Major version for the interface of the property as retreived from cache.
 * @return True when the property is found, false otherwise.
 */
static bool property_is_in_introspection(
    astarte_device_handle_t device, const char *interface_name, const char *path, uint32_t major);
/**
 * @brief Send the purge properties message for the device owned properties.
 *
 * @param[in] device Handle to the device instance.
 */
static void send_purge_device_properties(astarte_device_handle_t device);
#endif

/************************************************
 *         Global functions definitions         *
 ***********************************************/

astarte_result_t astarte_device_connection_connect(astarte_device_handle_t device)
{
    switch (device->connection_state) {
        case DEVICE_CONNECTING:
            ASTARTE_LOG_WRN("Called connect function when device is connecting.");
            return ASTARTE_RESULT_MQTT_CLIENT_ALREADY_CONNECTING;
        case DEVICE_CONNECTED:
            ASTARTE_LOG_WRN("Called connect function when device is already connected.");
            return ASTARTE_RESULT_MQTT_CLIENT_ALREADY_CONNECTED;
        default: // Other states: (DEVICE_DISCONNECTED)
            break;
    }

    astarte_result_t ares = astarte_mqtt_connect(&device->astarte_mqtt);
    if (ares == ASTARTE_RESULT_OK) {
        ASTARTE_LOG_DBG("Device connection state -> CONNECTING.");
        device->connection_state = DEVICE_CONNECTING;
    }
    return ares;
}

astarte_result_t astarte_device_connection_disconnect(astarte_device_handle_t device)
{
    if (device->connection_state == DEVICE_DISCONNECTED) {
        ASTARTE_LOG_ERR("Disconnection request for a disconnected client will be ignored.");
        return ASTARTE_RESULT_DEVICE_NOT_READY;
    }

    return astarte_mqtt_disconnect(&device->astarte_mqtt);
}

void astarte_device_connection_on_connected_handler(
    astarte_mqtt_t *astarte_mqtt, struct mqtt_connack_param connack_param)
{
    struct astarte_device *device = CONTAINER_OF(astarte_mqtt, struct astarte_device, astarte_mqtt);

    if (connack_param.session_present_flag != 0) {
#if defined(CONFIG_ASTARTE_DEVICE_SDK_PERMANENT_STORAGE)
        const char *intr_string = introspection_get_string(&device->introspection);
        astarte_result_t ares
            = astarte_device_caching_check_introspection(intr_string, strlen(intr_string) + 1);
        if (ares == ASTARTE_RESULT_OK) {
            ASTARTE_LOG_DBG("Device connection state -> CONNECTED.");
            device->connection_state = DEVICE_CONNECTED;
            return;
        }
#else
        ASTARTE_LOG_DBG("Device connection state -> CONNECTED.");
        device->connection_state = DEVICE_CONNECTED;
        return;
#endif
    }

    device->subscription_failure = false;
    setup_subscriptions(device);
    send_introspection(device);
    send_emptycache(device);
#if defined(CONFIG_ASTARTE_DEVICE_SDK_PERMANENT_STORAGE)
    send_device_owned_properties(device);
    send_purge_device_properties(device);
#endif

    ASTARTE_LOG_DBG("Device connection state -> CONNECTING.");
    device->connection_state = DEVICE_CONNECTING;
}

void astarte_device_connection_on_disconnected_handler(astarte_mqtt_t *astarte_mqtt)
{
    struct astarte_device *device = CONTAINER_OF(astarte_mqtt, struct astarte_device, astarte_mqtt);

    ASTARTE_LOG_DBG("Device connection state -> DISCONNECTED.");
    device->connection_state = DEVICE_DISCONNECTED;

    if (device->disconnection_cbk) {
        astarte_device_disconnection_event_t event = {
            .device = device,
            .user_data = device->cbk_user_data,
        };
        device->disconnection_cbk(event);
    }
}

void astarte_device_connection_on_subscribed_handler(
    astarte_mqtt_t *astarte_mqtt, uint16_t message_id, enum mqtt_suback_return_code return_code)
{
    (void) message_id;
    struct astarte_device *device = CONTAINER_OF(astarte_mqtt, struct astarte_device, astarte_mqtt);

    switch (return_code) {
        case MQTT_SUBACK_SUCCESS_QoS_0:
        case MQTT_SUBACK_SUCCESS_QoS_1:
        case MQTT_SUBACK_SUCCESS_QoS_2:
            break;
        case MQTT_SUBACK_FAILURE:
            device->subscription_failure = true;
            break;
        default:
            device->subscription_failure = true;
            ASTARTE_LOG_ERR("Invalid SUBACK return code.");
            break;
    }
}

astarte_result_t astarte_device_connection_poll(astarte_device_handle_t device)
{
    if (device->connection_state == DEVICE_CONNECTING) {
        if (device->subscription_failure) {
            ASTARTE_LOG_ERR("Subscription request has been denied, irrecoverable error.");
            return ASTARTE_RESULT_INTERNAL_ERROR;
        }
        if (!astarte_mqtt_has_pending_outgoing(&device->astarte_mqtt)) {

            ASTARTE_LOG_DBG("Device connection state -> CONNECTED.");
            device->connection_state = DEVICE_CONNECTED;

#if defined(CONFIG_ASTARTE_DEVICE_SDK_PERMANENT_STORAGE)
            const char *intr_string = introspection_get_string(&device->introspection);
            astarte_result_t ares
                = astarte_device_caching_store_introspection(intr_string, strlen(intr_string) + 1);
            ASTARTE_LOG_COND_ERR(ares != ASTARTE_RESULT_OK, "Store introspection failed: %s",
                astarte_result_to_name(ares));
#endif

            if (device->connection_cbk) {
                astarte_device_connection_event_t event = {
                    .device = device,
                    .user_data = device->cbk_user_data,
                };
                device->connection_cbk(event);
            }
        }
    }

    return astarte_mqtt_poll(&device->astarte_mqtt);
}

/************************************************
 *         Static functions definitions         *
 ***********************************************/

static void setup_subscriptions(astarte_device_handle_t device)
{
    const char *topic = device->control_consumer_prop_topic;
    uint16_t message_id = 0;
    ASTARTE_LOG_DBG("Subscribing to: %s", topic);
    astarte_mqtt_subscribe(&device->astarte_mqtt, topic, 2, &message_id);

    for (introspection_node_t *iterator = introspection_iter(&device->introspection);
         iterator != NULL; iterator = introspection_iter_next(&device->introspection, iterator)) {
        const astarte_interface_t *interface = iterator->interface;

        if (interface->ownership == ASTARTE_INTERFACE_OWNERSHIP_SERVER) {
            char *topic = NULL;
            int ret = asprintf(&topic, CONFIG_ASTARTE_DEVICE_SDK_REALM_NAME "/%s/%s/#",
                device->device_id, interface->name);
            if (ret < 0) {
                ASTARTE_LOG_ERR("Error encoding MQTT topic");
                ASTARTE_LOG_ERR("Might be out of memory %s: %d", __FILE__, __LINE__);
                continue;
            }

            ASTARTE_LOG_DBG("Subscribing to: %s", topic);
            astarte_mqtt_subscribe(&device->astarte_mqtt, topic, 2, &message_id);
            free(topic);
        }
    }
}

static void send_introspection(astarte_device_handle_t device)
{
    const char *topic = device->base_topic;
    char *intr_str = (void *) introspection_get_string(&device->introspection);
    uint16_t message_id = 0;
    ASTARTE_LOG_DBG("Publishing introspection: %s", intr_str);
    astarte_mqtt_publish(&device->astarte_mqtt, topic, intr_str, strlen(intr_str), 2, &message_id);
}

static void send_emptycache(astarte_device_handle_t device)
{
    const char *topic = device->control_empty_cache_topic;
    uint16_t message_id = 0;
    ASTARTE_LOG_DBG("Sending emptyCache to %s", topic);
    astarte_mqtt_publish(&device->astarte_mqtt, topic, "1", strlen("1"), 2, &message_id);
}

#if defined(CONFIG_ASTARTE_DEVICE_SDK_PERMANENT_STORAGE)
static void send_device_owned_properties(astarte_device_handle_t device)
{
    astarte_result_t ares = ASTARTE_RESULT_OK;
    char *interface_name = NULL;
    char *path = NULL;
    astarte_individual_t individual = { 0 };

    astarte_device_caching_property_iter_t iter = { 0 };
    ares = astarte_device_caching_property_iterator_init(&iter);
    if ((ares != ASTARTE_RESULT_OK) && (ares != ASTARTE_RESULT_NOT_FOUND)) {
        ASTARTE_LOG_ERR("Properties iterator init failed: %s", astarte_result_to_name(ares));
        goto end;
    }

    while (ares != ASTARTE_RESULT_NOT_FOUND) {
        size_t interface_name_size = 0U;
        size_t path_size = 0U;
        ares = astarte_device_caching_property_iterator_get(
            &iter, NULL, &interface_name_size, NULL, &path_size);
        if (ares != ASTARTE_RESULT_OK) {
            ASTARTE_LOG_ERR("Properties iterator get error: %s", astarte_result_to_name(ares));
            goto end;
        }

        // Allocate space for the name and path
        char *interface_name = calloc(interface_name_size, sizeof(char));
        char *path = calloc(path_size, sizeof(char));
        if (!interface_name || !path) {
            ASTARTE_LOG_ERR("Out of memory %s: %d", __FILE__, __LINE__);
            goto end;
        }

        ares = astarte_device_caching_property_iterator_get(
            &iter, interface_name, &interface_name_size, path, &path_size);
        if (ares != ASTARTE_RESULT_OK) {
            ASTARTE_LOG_ERR("Properties iterator get error: %s", astarte_result_to_name(ares));
            goto end;
        }

        uint32_t major = 0U;
        ares = astarte_device_caching_load_property(interface_name, path, &major, &individual);
        if (ares != ASTARTE_RESULT_OK) {
            ASTARTE_LOG_ERR("Properties load property error: %s", astarte_result_to_name(ares));
            goto end;
        }

        if (property_is_in_introspection(device, interface_name, path, major)) {
            ares = astarte_device_stream_individual(device, interface_name, path, individual, NULL);
            ASTARTE_LOG_COND_ERR(ares != ASTARTE_RESULT_OK, "Failed sending cached property: %s",
                astarte_result_to_name(ares));
        }

        free(interface_name);
        interface_name = NULL;
        free(path);
        path = NULL;
        astarte_device_caching_destroy_loaded_property(individual);
        individual = (astarte_individual_t){ 0 };

        ares = astarte_device_caching_property_iterator_next(&iter);
        if ((ares != ASTARTE_RESULT_OK) && (ares != ASTARTE_RESULT_NOT_FOUND)) {
            ASTARTE_LOG_ERR("Iterator next error: %s", astarte_result_to_name(ares));
            goto end;
        }
    }

end:
    // Free all data
    free(interface_name);
    free(path);
    astarte_device_caching_destroy_loaded_property(individual);
}

static bool property_is_in_introspection(
    astarte_device_handle_t device, const char *interface_name, const char *path, uint32_t major)
{
    introspection_node_t *iter = introspection_iter(&device->introspection);

    while (iter) {
        const astarte_interface_t *interface = iter->interface;
        if ((strcmp(interface->name, interface_name) == 0) && (interface->major_version == major)) {
            return true;
        }
        iter = introspection_iter_next(&device->introspection, iter);
    }

    // If property is not in introspection, delete it
    astarte_result_t ares = astarte_device_caching_delete_property(interface_name, path);
    ASTARTE_LOG_COND_ERR(ares != ASTARTE_RESULT_OK, "Failed deleting the cached property: %s",
        astarte_result_to_name(ares));

    return false;
}

static void send_purge_device_properties(astarte_device_handle_t device)
{
    astarte_result_t ares = ASTARTE_RESULT_OK;
    char *string = NULL;

    size_t string_size = 0U;
    ares = astarte_device_caching_get_properties_string(NULL, &string_size);
    if (ares != ASTARTE_RESULT_OK) {
        ASTARTE_LOG_ERR("Error getting cached properties string: %s", astarte_result_to_name(ares));
        goto exit;
    }
    if (string_size == 0) {
        // TODO: evaluate if we should send an empty string in this case
        goto exit;
    }

    string = calloc(string_size, sizeof(char));
    if (!string) {
        ASTARTE_LOG_ERR("Out of memory %s: %d", __FILE__, __LINE__);
        goto exit;
    }

    ares = astarte_device_caching_get_properties_string(string, &string_size);
    if (ares != ASTARTE_RESULT_OK) {
        ASTARTE_LOG_ERR("Error getting cached properties string: %s", astarte_result_to_name(ares));
        goto exit;
    }

    // TODO encode the string to transmit

    // TODO transmit the payload


exit:
    free(string);
}
#endif
