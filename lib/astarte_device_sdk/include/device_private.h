/*
 * (C) Copyright 2024, SECO Mind Srl
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef DEVICE_PRIVATE_H
#define DEVICE_PRIVATE_H

/**
 * @file device_private.h
 * @brief Device private header.
 */

#include "astarte_device_sdk/astarte.h"
#include "astarte_device_sdk/device.h"
#include "astarte_device_sdk/result.h"

#include "introspection.h"
#include "mqtt.h"
#include "tls_credentials.h"

/** @brief Generic prefix to be used for all MQTT topics. */
#define MQTT_TOPIC_PREFIX CONFIG_ASTARTE_DEVICE_SDK_REALM_NAME "/"
/** @brief Generic suffix to be used for all control MQTT topics. */
#define MQTT_CONTROL_TOPIC_SUFFIX "/control"
/** @brief Suffix to be used for the consumer properties control MQTT topic. */
#define MQTT_CONTROL_CONSUMER_PROP_TOPIC_SUFFIX MQTT_CONTROL_TOPIC_SUFFIX "/consumer/properties"
/** @brief Suffix to be used for the empty cache control MQTT topic. */
#define MQTT_CONTROL_EMPTY_CACHE_TOPIC_SUFFIX MQTT_CONTROL_TOPIC_SUFFIX "/emptyCache"

/** @brief Size in chars of the #MQTT_TOPIC_PREFIX string. */
#define MQTT_TOPIC_PREFIX_LEN (sizeof(MQTT_TOPIC_PREFIX) - 1)
/** @brief Size in chars of the generic base topic. In the form: 'REALM NAME/DEVICE ID' */
#define MQTT_BASE_TOPIC_LEN (MQTT_TOPIC_PREFIX_LEN + ASTARTE_PAIRING_DEVICE_ID_LEN)
/** @brief Size in chars of the #MQTT_CONTROL_TOPIC_SUFFIX string. */
#define MQTT_CONTROL_TOPIC_SUFFIX_LEN (sizeof(MQTT_CONTROL_TOPIC_SUFFIX) - 1)
/** @brief Size in chars of the generic control topic. */
#define MQTT_CONTROL_TOPIC_LEN (MQTT_BASE_TOPIC_LEN + MQTT_CONTROL_TOPIC_SUFFIX_LEN)
/** @brief Size in chars of the #MQTT_CONTROL_CONSUMER_PROP_TOPIC_SUFFIX string. */
#define MQTT_CONTROL_CONSUMER_PROP_TOPIC_SUFFIX_LEN                                                \
    (sizeof(MQTT_CONTROL_CONSUMER_PROP_TOPIC_SUFFIX) - 1)
/** @brief Size in chars of the consumer properties control topic. */
#define MQTT_CONTROL_CONSUMER_PROP_TOPIC_LEN                                                       \
    (MQTT_BASE_TOPIC_LEN + MQTT_CONTROL_CONSUMER_PROP_TOPIC_SUFFIX_LEN)
/** @brief Size in chars of the #MQTT_CONTROL_EMPTY_CACHE_TOPIC_SUFFIX string. */
#define MQTT_CONTROL_EMPTY_CACHE_TOPIC_SUFFIX_LEN                                                  \
    (sizeof(MQTT_CONTROL_EMPTY_CACHE_TOPIC_SUFFIX) - 1)
/** @brief Size in chars of the empty cache control topic. */
#define MQTT_CONTROL_EMPTY_CACHE_TOPIC_LEN                                                         \
    (MQTT_BASE_TOPIC_LEN + MQTT_CONTROL_EMPTY_CACHE_TOPIC_SUFFIX_LEN)

/** @brief Connection statuses for the Astarte device. */
enum connection_states
{
    /** @brief The device has never been connected or has been disconnected. */
    DEVICE_DISCONNECTED = 0U,
    /** @brief The device is connecting to Astarte. */
    DEVICE_CONNECTING,
    /** @brief The device has fully connected to Astarte. */
    DEVICE_CONNECTED,
};

/**
 * @brief Internal struct for an instance of an Astarte device.
 *
 * @warning Users should not modify the content of this struct directly
 */
struct astarte_device
{
    /** @brief Timeout for http requests. */
    int32_t http_timeout_ms;
    /** @brief Private client key and certificate for mutual TLS authentication (PEM format). */
    astarte_tls_credentials_client_crt_t client_crt;
    /** @brief Unique 128 bits, base64 URL encoded, identifier to associate to a device instance. */
    char device_id[ASTARTE_PAIRING_DEVICE_ID_LEN + 1];
    /** @brief Device's credential secret. */
    char cred_secr[ASTARTE_PAIRING_CRED_SECR_LEN + 1];
    /** @brief MQTT client handle. */
    astarte_mqtt_t astarte_mqtt;
    /** @brief Device introspection. */
    introspection_t introspection;
    /** @brief (optional) User callback for connection events. */
    astarte_device_connection_cbk_t connection_cbk;
    /** @brief (optional) User callback for disconnection events. */
    astarte_device_disconnection_cbk_t disconnection_cbk;
    /** @brief (optional) User callback for incoming datastream individual events. */
    astarte_device_datastream_individual_cbk_t datastream_individual_cbk;
    /** @brief (optional) User callback for incoming datastream object events. */
    astarte_device_datastream_object_cbk_t datastream_object_cbk;
    /** @brief (optional) User callback for incoming property set events. */
    astarte_device_property_set_cbk_t property_set_cbk;
    /** @brief (optional) User callback for incoming property unset events. */
    astarte_device_property_unset_cbk_t property_unset_cbk;
    /** @brief (optional) User data to pass to all the set callbacks. */
    void *cbk_user_data;
    /** @brief Connection state of the Astarte device. */
    enum connection_states connection_state;
    /** @brief Base MQTT topic for the device. */
    char base_topic[MQTT_BASE_TOPIC_LEN + 1];
    /** @brief Base MQTT control topic for the device. */
    char control_topic[MQTT_CONTROL_TOPIC_LEN + 1];
    /** @brief Subscription topic for control consumer properties. */
    char control_consumer_prop_topic[MQTT_CONTROL_CONSUMER_PROP_TOPIC_LEN + 1];
    /** @brief Publish topic for the control EmptyCache. */
    char control_empty_cache_topic[MQTT_CONTROL_EMPTY_CACHE_TOPIC_LEN + 1];
};

#ifdef __cplusplus
extern "C" {
#endif

#ifdef __cplusplus
}
#endif

#endif // DEVICE_PRIVATE_H
