/*
 * (C) Copyright 2024, SECO Mind Srl
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <string.h>

#include <zephyr/data/json.h>
#include <zephyr/kernel.h>
#include <zephyr/net/http/client.h>
#include <zephyr/net/socket.h>
#include <zephyr/posix/arpa/inet.h>

#if (!defined(CONFIG_ASTARTE_DEVICE_SDK_DEVELOP_USE_NON_TLS_HTTP)                                  \
    || !defined(CONFIG_ASTARTE_DEVICE_SDK_DEVELOP_USE_NON_TLS_MQTT))
#include "ca_certificates.h"
#include <zephyr/net/tls_credentials.h>
#endif

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(main, CONFIG_APP_LOG_LEVEL); // NOLINT

#include <astarte_device_sdk/device.h>
#include <astarte_device_sdk/individual.h>
#include <astarte_device_sdk/interface.h>
#include <astarte_device_sdk/mapping.h>
#include <astarte_device_sdk/pairing.h>

#if defined(CONFIG_WIFI)
#include "wifi.h"
#else
#include "eth.h"
#endif
#include "utils.h"

#include "generated_interfaces.h"

/************************************************
 *       Checks over configuration values       *
 ***********************************************/

BUILD_ASSERT(sizeof(CONFIG_DEVICE_ID) == ASTARTE_DEVICE_ID_LEN + 1,
    "Missing device ID in datastreams example");
BUILD_ASSERT(sizeof(CONFIG_CREDENTIAL_SECRET) == ASTARTE_PAIRING_CRED_SECR_LEN + 1,
    "Missing credential secret in datastreams example");

/************************************************
 * Constants, static variables and defines
 ***********************************************/

#define MAIN_THREAD_SLEEP_MS 500

#define DEVICE_RX_THREAD_FLAGS_TERMINATION 1U
// NOLINTBEGIN(cppcoreguidelines-avoid-non-const-global-variables)
static atomic_t device_rx_thread_flags;
K_THREAD_STACK_DEFINE(device_rx_thread_stack_area, CONFIG_DEVICE_RX_THREAD_STACK_SIZE);
static struct k_thread device_rx_thread_data;

K_THREAD_STACK_DEFINE(device_tx_thread_stack_area, CONFIG_DEVICE_TX_THREAD_STACK_SIZE);
static struct k_thread device_tx_thread_data;
// NOLINTEND(cppcoreguidelines-avoid-non-const-global-variables)

/************************************************
 * Static functions declaration
 ***********************************************/

/**
 * @brief Entry point for the Astarte device reception thread.
 *
 * @param device_handle Handle to the Astarte device.
 * @param unused1 Unused parameter.
 * @param unused2 Unused parameter.
 */
static void device_rx_thread_entry_point(void *device_handle, void *unused1, void *unused2);
/**
 * @brief Entry point for the Astarte device transmission thread.
 *
 * @param device_handle Handle to the Astarte device.
 * @param unused1 Unused parameter.
 * @param unused2 Unused parameter.
 */
static void device_tx_thread_entry_point(void *device_handle, void *unused1, void *unused2);
/**
 * @brief Callback handler for Astarte connection events.
 *
 * @param event Astarte device connection event.
 */
static void connection_callback(astarte_device_connection_event_t event);
/**
 * @brief Callback handler for Astarte disconnection events.
 *
 * @param event Astarte device disconnection event.
 */
static void disconnection_callback(astarte_device_disconnection_event_t event);
/**
 * @brief Handler for astarte set property event.
 *
 * @param event Astarte device data event pointer.
 */
static void properties_set_events_handler(astarte_device_property_set_event_t event);
/**
 * @brief Handler for astarte unset property event.
 *
 * @param event Astarte device data event pointer.
 */
static void properties_unset_events_handler(astarte_device_data_event_t event);

/************************************************
 * Global functions definition
 ***********************************************/

int main(void)
{
    LOG_INF("Astarte device sample"); // NOLINT
    LOG_INF("Board: %s", CONFIG_BOARD); // NOLINT

    // Initialize WiFi/Ethernet driver
#if defined(CONFIG_WIFI)
    LOG_INF("Initializing WiFi driver."); // NOLINT
    wifi_init();
#else
    LOG_INF("Initializing Ethernet driver."); // NOLINT
    if (eth_connect() != 0) {
        LOG_ERR("Connectivity intialization failed!"); // NOLINT
        return -1;
    }
#endif

    // Add TLS certificate if required
#if (!defined(CONFIG_ASTARTE_DEVICE_SDK_DEVELOP_USE_NON_TLS_HTTP)                                  \
    || !defined(CONFIG_ASTARTE_DEVICE_SDK_DEVELOP_USE_NON_TLS_MQTT))
    tls_credential_add(CONFIG_ASTARTE_DEVICE_SDK_HTTPS_CA_CERT_TAG, TLS_CREDENTIAL_CA_CERTIFICATE,
        ca_certificate_root, sizeof(ca_certificate_root));
#endif

    // Create a new instance of an Astarte device
    char device_id[ASTARTE_DEVICE_ID_LEN + 1] = CONFIG_DEVICE_ID;
    char cred_secr[ASTARTE_PAIRING_CRED_SECR_LEN + 1] = CONFIG_CREDENTIAL_SECRET;

    const astarte_interface_t *interfaces[] = { &org_astarteplatform_zephyr_examples_DeviceProperty,
        &org_astarteplatform_zephyr_examples_ServerProperty };

    astarte_device_config_t device_config = { 0 };
    device_config.http_timeout_ms = CONFIG_HTTP_TIMEOUT_MS;
    device_config.mqtt_connection_timeout_ms = CONFIG_MQTT_CONNECTION_TIMEOUT_MS;
    device_config.mqtt_poll_timeout_ms = CONFIG_MQTT_POLL_TIMEOUT_MS;
    device_config.connection_cbk = connection_callback;
    device_config.disconnection_cbk = disconnection_callback;
    device_config.property_set_cbk = properties_set_events_handler;
    device_config.property_unset_cbk = properties_unset_events_handler;
    device_config.interfaces = interfaces;
    device_config.interfaces_size = ARRAY_SIZE(interfaces);
    memcpy(device_config.device_id, device_id, sizeof(device_id));
    memcpy(device_config.cred_secr, cred_secr, sizeof(cred_secr));

    astarte_device_handle_t device = NULL;
    astarte_result_t res = astarte_device_new(&device_config, &device);
    if (res != ASTARTE_RESULT_OK) {
        LOG_ERR("Astarte device creation failure."); // NOLINT
        return -1;
    }

    // Spawn a new thread for the Astarte device
    k_thread_create(&device_rx_thread_data, device_rx_thread_stack_area,
        K_THREAD_STACK_SIZEOF(device_rx_thread_stack_area), device_rx_thread_entry_point,
        (void *) device, NULL, NULL, CONFIG_DEVICE_THREAD_PRIORITY, 0, K_NO_WAIT);
    k_thread_create(&device_tx_thread_data, device_tx_thread_stack_area,
        K_THREAD_STACK_SIZEOF(device_tx_thread_stack_area), device_tx_thread_entry_point,
        (void *) device, NULL, NULL, CONFIG_DEVICE_THREAD_PRIORITY, 0, K_NO_WAIT);

    // Wait for a predefined operational time.
    k_timepoint_t disconnect_timepoint
        = sys_timepoint_calc(K_SECONDS(CONFIG_DEVICE_OPERATIONAL_TIME_SECONDS));
    while (!K_TIMEOUT_EQ(sys_timepoint_timeout(disconnect_timepoint), K_NO_WAIT)) {
// Ensure the connectivity is still present
#if defined(CONFIG_WIFI)
        wifi_poll();
#else
        eth_poll();
#endif

        k_sleep(K_MSEC(MAIN_THREAD_SLEEP_MS));
    }

    // Signal to the Astarte thread that is should terminate.
    atomic_set_bit(&device_rx_thread_flags, DEVICE_RX_THREAD_FLAGS_TERMINATION);

    // Wait for the Astarte thread to terminate.
    if (k_thread_join(&device_rx_thread_data, K_FOREVER) != 0) {
        LOG_ERR("Failed in waiting for the Astarte thread to terminate."); // NOLINT
    }

    LOG_INF("Astarte device sample finished."); // NOLINT
    k_sleep(K_MSEC(MSEC_PER_SEC));

    return 0;
}

/************************************************
 * Static functions definitions
 ***********************************************/

static void device_rx_thread_entry_point(void *device_handle, void *unused1, void *unused2)
{
    (void) unused1;
    (void) unused2;
    astarte_result_t res = ASTARTE_RESULT_OK;

    astarte_device_handle_t device = (astarte_device_handle_t) device_handle;
    res = astarte_device_connect(device);
    if (res != ASTARTE_RESULT_OK) {
        LOG_ERR("Astarte device connection failure."); // NOLINT
        return;
    }

    res = astarte_device_poll(device);
    if (res != ASTARTE_RESULT_OK) {
        // First poll should not timeout as we should receive a connection ack.
        LOG_ERR("Astarte device first poll failure."); // NOLINT
        return;
    }

    while (!atomic_test_bit(&device_rx_thread_flags, DEVICE_RX_THREAD_FLAGS_TERMINATION)) {
        k_timepoint_t timepoint = sys_timepoint_calc(K_MSEC(CONFIG_DEVICE_POLL_PERIOD_MS));

        res = astarte_device_poll(device);
        if (res != ASTARTE_RESULT_OK) {
            LOG_ERR("Astarte device poll failure."); // NOLINT
            return;
        }

        k_sleep(sys_timepoint_timeout(timepoint));
    }

    LOG_INF("End of loop, disconnection imminent."); // NOLINT

    res = astarte_device_disconnect(device);
    if (res != ASTARTE_RESULT_OK) {
        LOG_ERR("Astarte device disconnection failure."); // NOLINT
        return;
    }

    LOG_INF("Astarte thread will now be terminated."); // NOLINT

    k_sleep(K_MSEC(MSEC_PER_SEC));
}

static void device_tx_thread_entry_point(void *device_handle, void *unused1, void *unused2)
{
    astarte_result_t res = ASTARTE_RESULT_OK;
    astarte_device_handle_t device = (astarte_device_handle_t) device_handle;

    (void) unused1;
    (void) unused2;

    const char *interface_name = org_astarteplatform_zephyr_examples_DeviceProperty.name;

    const char *paths[] = {
        "/sensor44/binaryblob_endpoint",
        "/sensor44/binaryblobarray_endpoint",
        "/sensor44/boolean_endpoint",
        "/sensor44/booleanarray_endpoint",
        "/sensor44/datetime_endpoint",
        "/sensor44/datetimearray_endpoint",
        "/sensor44/double_endpoint",
        "/sensor44/doublearray_endpoint",
        "/sensor44/integer_endpoint",
        "/sensor44/integerarray_endpoint",
        "/sensor44/longinteger_endpoint",
        "/sensor44/longintegerarray_endpoint",
        "/sensor44/string_endpoint",
        "/sensor44/stringarray_endpoint",
    };

    // Wait for the predefined time
    k_sleep(K_SECONDS(CONFIG_DEVICE_SET_PROPERTIES_DELAY_SECONDS));

    LOG_INF("Setting some properties using the Astarte device."); // NOLINT

    astarte_individual_t individuals[]
        = { astarte_individual_from_binaryblob(
                (void *) utils_binary_blob_data, ARRAY_SIZE(utils_binary_blob_data)),
              astarte_individual_from_binaryblob_array((const void **) utils_binary_blobs_data,
                  (size_t *) utils_binary_blobs_sizes_data, ARRAY_SIZE(utils_binary_blobs_data)),
              astarte_individual_from_boolean(utils_boolean_data),
              astarte_individual_from_boolean_array(
                  (bool *) utils_boolean_array_data, ARRAY_SIZE(utils_boolean_array_data)),
              astarte_individual_from_datetime(utils_unix_time_data),
              astarte_individual_from_datetime_array(
                  (int64_t *) utils_unix_time_array_data, ARRAY_SIZE(utils_unix_time_array_data)),
              astarte_individual_from_double(utils_double_data),
              astarte_individual_from_double_array(
                  (double *) utils_double_array_data, ARRAY_SIZE(utils_double_array_data)),
              astarte_individual_from_integer(utils_integer_data),
              astarte_individual_from_integer_array(
                  (int32_t *) utils_integer_array_data, ARRAY_SIZE(utils_integer_array_data)),
              astarte_individual_from_longinteger(utils_longinteger_data),
              astarte_individual_from_longinteger_array((int64_t *) utils_longinteger_array_data,
                  ARRAY_SIZE(utils_longinteger_array_data)),
              astarte_individual_from_string(utils_string_data),
              astarte_individual_from_string_array(
                  (const char **) utils_string_array_data, ARRAY_SIZE(utils_string_array_data)) };

    for (size_t i = 0; i < ARRAY_SIZE(individuals); i++) {
        LOG_INF("Setting on %s:", paths[i]); // NOLINT
        utils_log_astarte_individual(individuals[i]);
        res = astarte_device_set_property(device, interface_name, paths[i], individuals[i]);
        if (res != ASTARTE_RESULT_OK) {
            LOG_INF("Astarte device transmission failure."); // NOLINT
        }
    }

    LOG_INF("Setting properties completed."); // NOLINT

    // Wait for the predefined time
    k_sleep(K_SECONDS(CONFIG_DEVICE_UNSET_PROPERTIES_DELAY_SECONDS));

    LOG_INF("Unsetting some properties using the Astarte device."); // NOLINT

    for (size_t i = 0; i < ARRAY_SIZE(individuals); i++) {
        LOG_INF("Unsetting %s:", paths[i]); // NOLINT
        res = astarte_device_unset_property(device, interface_name, paths[i]);
        if (res != ASTARTE_RESULT_OK) {
            LOG_INF("Astarte device transmission failure."); // NOLINT
        }
    }

    LOG_INF("Unsetting properties completed."); // NOLINT
}

static void connection_callback(astarte_device_connection_event_t event)
{
    (void) event;
    LOG_INF("Astarte device connected."); // NOLINT
}

static void disconnection_callback(astarte_device_disconnection_event_t event)
{
    (void) event;
    LOG_INF("Astarte device disconnected"); // NOLINT
}

static void properties_set_events_handler(astarte_device_property_set_event_t event)
{
    const char *interface_name = event.data_event.interface_name;
    const char *path = event.data_event.path;
    astarte_individual_t individual = event.individual;

    LOG_INF("Property set event, interface: %s, path: %s", interface_name, path); // NOLINT

    if (strcmp(interface_name, org_astarteplatform_zephyr_examples_ServerProperty.name) == 0) {
        // Pretty log the received individual
        utils_log_astarte_individual(individual);
    }
}

static void properties_unset_events_handler(astarte_device_data_event_t event)
{
    const char *interface_name = event.interface_name;
    const char *path = event.path;
    LOG_INF("Property unset event, interface: %s, path: %s", interface_name, path); // NOLINT
}
