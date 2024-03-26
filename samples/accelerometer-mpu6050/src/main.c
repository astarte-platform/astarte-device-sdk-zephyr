/*
 * (C) Copyright 2024, SECO Mind Srl
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <string.h>

#include <zephyr/data/json.h>
#include <zephyr/device.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/net/http/client.h>
#include <zephyr/net/socket.h>
#include <zephyr/posix/arpa/inet.h>

#if !defined(CONFIG_SDK_DEVELOP_DISABLE_OR_IGNORE_TLS)
#include "ca_certificates.h"
#include <zephyr/net/tls_credentials.h>
#endif

#include <astarte_device_sdk/device.h>
#include <astarte_device_sdk/interface.h>
#include <astarte_device_sdk/pairing.h>
#include <astarte_device_sdk/value.h>

#if defined(CONFIG_WIFI)
#include "wifi.h"
#else
#include "eth.h"
#endif

#include "generated_interfaces.h"

LOG_MODULE_REGISTER(main, CONFIG_APP_LOG_LEVEL); // NOLINT

/************************************************
 *       Checks over configuration values       *
 ***********************************************/

BUILD_ASSERT(sizeof(CONFIG_CREDENTIAL_SECRET) == ASTARTE_PAIRING_CRED_SECR_LEN + 1,
    "Missing credential secret in aggregates example");

/************************************************
 * Constants and defines
 ***********************************************/

#define MQTT_POLL_TIMEOUT_MS 200
#define DEVICE_OPERATIONAL_TIME_MS (60 * MSEC_PER_SEC)

/************************************************
 * Static functions declaration
 ***********************************************/

/**
 * @brief Handler for astarte connection events.
 *
 * @param event Astarte device connection event pointer.
 */
static void connection_events_handler(astarte_device_connection_event_t event);
/**
 * @brief Handler for astarte disconnection events.
 *
 * @param event Astarte device disconnection event pointer.
 */
static void disconnection_events_handler(astarte_device_disconnection_event_t event);
/**
 * @brief Handler for astarte datastream object event.
 *
 * @param event Astarte device datastream object event pointer.
 */
static void datastream_object_events_handler(astarte_device_datastream_object_event_t event);
/**
 * @brief Parse the received BSON data for this example.
 *
 * @param[in] rx_values Array of received key-value pairs
 * @param[in] rx_values_length Size of the array rx_values
 */
static void parse_received_data(astarte_value_pair_t *rx_values, size_t rx_values_length);
/**
 * @brief Stream a predefined reply to Astarte.
 *
 * @param event Astarte device datastream object event pointer.
 */
static void stream_reply(astarte_device_data_event_t event);

/************************************************
 * Global functions definition
 ***********************************************/

static const char *now_str(void)
{
	static char buf[16]; /* ...HH:MM:SS.MMM */
	uint32_t now = k_uptime_get_32();
	unsigned int ms = now % MSEC_PER_SEC;
	unsigned int s;
	unsigned int min;
	unsigned int h;

	now /= MSEC_PER_SEC;
	s = now % 60U;
	now /= 60U;
	min = now % 60U;
	now /= 60U;
	h = now;

	snprintf(buf, sizeof(buf), "%u:%02u:%02u.%03u",
		 h, min, s, ms);
	return buf;
}

static int process_mpu6050(const struct device *dev)
{
	struct sensor_value temperature;
	struct sensor_value accel[3];
	struct sensor_value gyro[3];
	int rc = sensor_sample_fetch(dev);

	if (rc == 0) {
		rc = sensor_channel_get(dev, SENSOR_CHAN_ACCEL_XYZ,
					accel);
	}
	if (rc == 0) {
		rc = sensor_channel_get(dev, SENSOR_CHAN_GYRO_XYZ,
					gyro);
	}
	if (rc == 0) {
		rc = sensor_channel_get(dev, SENSOR_CHAN_DIE_TEMP,
					&temperature);
	}
	if (rc == 0) {
		printf("[%s]:%g Cel\n"
		       "  accel %f %f %f m/s/s\n"
		       "  gyro  %f %f %f rad/s\n",
		       now_str(),
		       sensor_value_to_double(&temperature),
		       sensor_value_to_double(&accel[0]),
		       sensor_value_to_double(&accel[1]),
		       sensor_value_to_double(&accel[2]),
		       sensor_value_to_double(&gyro[0]),
		       sensor_value_to_double(&gyro[1]),
		       sensor_value_to_double(&gyro[2]));
	} else {
		printf("sample fetch/get failed: %d\n", rc);
	}

	return rc;
}

int main(void)
{
    astarte_result_t res = ASTARTE_RESULT_OK;
    LOG_INF("MQTT Example\nBoard: %s", CONFIG_BOARD); // NOLINT

    const struct device *const mpu6050 = DEVICE_DT_GET_ONE(invensense_mpu6050);

    if (!device_is_ready(mpu6050)) {
        printf("Device %s is not ready\n", mpu6050->name);
        return 0;
    }

    while (1) {
        /* code */

        int rc = process_mpu6050(mpu6050);

        if (rc != 0) {
            break;
        }
        k_sleep(K_SECONDS(2));
    }

    //     // Initialize WiFi driver
    // #if defined(CONFIG_WIFI)
    //     LOG_INF("Initializing WiFi driver."); // NOLINT
    //     wifi_init();
    // #else
    //     if (eth_connect() != 0) {
    //         LOG_ERR("Connectivity intialization failed!"); // NOLINT
    //         return -1;
    //     }
    // #endif

    //     k_sleep(K_SECONDS(5)); // sleep for 5 seconds

    // #if !defined(CONFIG_ASTARTE_DEVICE_SDK_DEVELOP_DISABLE_OR_IGNORE_TLS)
    //     tls_credential_add(CONFIG_ASTARTE_DEVICE_SDK_CA_CERT_TAG, TLS_CREDENTIAL_CA_CERTIFICATE,
    //         ca_certificate_root, sizeof(ca_certificate_root));
    // #endif

    //     int32_t timeout_ms = 3 * MSEC_PER_SEC;
    //     char cred_secr[ASTARTE_PAIRING_CRED_SECR_LEN + 1] = CONFIG_CREDENTIAL_SECRET;

    //     const astarte_interface_t *interfaces[]
    //         = { &org_astarteplatform_zephyr_examples_DeviceAggregate,
    //               &org_astarteplatform_zephyr_examples_ServerAggregate };

    //     astarte_device_config_t device_config;
    //     memset(&device_config, 0, sizeof(device_config));
    //     device_config.http_timeout_ms = timeout_ms;
    //     device_config.mqtt_connection_timeout_ms = timeout_ms;
    //     device_config.mqtt_connected_timeout_ms = MQTT_POLL_TIMEOUT_MS;
    //     device_config.connection_cbk = connection_events_handler;
    //     device_config.disconnection_cbk = disconnection_events_handler;
    //     device_config.datastream_object_cbk = datastream_object_events_handler;
    //     device_config.interfaces = interfaces;
    //     device_config.interfaces_size = ARRAY_SIZE(interfaces);
    //     memcpy(device_config.cred_secr, cred_secr, sizeof(cred_secr));

    //     astarte_device_handle_t device = NULL;
    //     res = astarte_device_new(&device_config, &device);
    //     if (res != ASTARTE_RESULT_OK) {
    //         return -1;
    //     }

    //     res = astarte_device_connect(device);
    //     if (res != ASTARTE_RESULT_OK) {
    //         return -1;
    //     }

    //     res = astarte_device_poll(device);
    //     if (res != ASTARTE_RESULT_OK) {
    //         LOG_ERR("First poll should not timeout as we should receive a connection ack."); //
    //         NOLINT return -1;
    //     }

    //     k_timepoint_t disconnect_timepoint =
    //     sys_timepoint_calc(K_MSEC(DEVICE_OPERATIONAL_TIME_MS)); int count = 0; while (1) {
    //         k_timepoint_t timepoint = sys_timepoint_calc(K_MSEC(MQTT_POLL_TIMEOUT_MS));

    //         res = astarte_device_poll(device);
    //         if ((res != ASTARTE_RESULT_TIMEOUT) && (res != ASTARTE_RESULT_OK)) {
    //             return -1;
    //         }

    //         if (++count % (CONFIG_SLEEP_MS / MQTT_POLL_TIMEOUT_MS) == 0) {
    //             LOG_INF("Polling mqtt events... %s", CONFIG_BOARD); // NOLINT
    //             count = 0;
    //         }
    //         k_sleep(sys_timepoint_timeout(timepoint));

    //         if (K_TIMEOUT_EQ(sys_timepoint_timeout(disconnect_timepoint), K_NO_WAIT)) {
    //             break;
    //         }
    //     }

    //     LOG_INF("End of loop, disconnection imminent %s", CONFIG_BOARD); // NOLINT

    //     res = astarte_device_destroy(device);
    //     if (res != ASTARTE_RESULT_OK) {
    //         LOG_ERR("Failed destroying the device."); // NOLINT
    //         return -1;
    //     }

    return 0;
}

/************************************************
 * Static functions definitions
 ***********************************************/

static void connection_events_handler(astarte_device_connection_event_t event)
{
    LOG_INF("Astarte device connected, session_present: %d", event.session_present); // NOLINT
}

static void disconnection_events_handler(astarte_device_disconnection_event_t event)
{
    (void) event;
    LOG_INF("Astarte device disconnected"); // NOLINT
}

static void datastream_object_events_handler(astarte_device_datastream_object_event_t event)
{
    astarte_device_data_event_t rx_event = event.data_event;
    astarte_value_pair_t *rx_values = event.values;
    size_t rx_values_length = event.values_length;
    // NOLINTNEXTLINE
    LOG_INF("Got Astarte datastream object event, interface_name: %s, path: %s",
        rx_event.interface_name, rx_event.path);

    LOG_INF("Device aggregate sent, using sensor_id: sensor24."); // NOLINT
}

static void stream_reply(astarte_device_data_event_t event)
{

    LOG_INF("Sending device aggregate with the following content:"); // NOLINT

    // NOLINTBEGIN(readability-magic-numbers, cppcoreguidelines-avoid-magic-numbers)
    // double double_endpoint = 43.2;
    // int32_t integer_endpoint = 54;
    // bool boolean_endpoint = true;
    // int64_t longinteger_endpoint = 45993543534;
    // char string_endpoint[] = "hello world";
    // uint8_t binaryblob_endpoint[] = { 0x12, 0x11, 0x32 };
    // int64_t datetime_endpoint = 1710157717000; // Needs to be in ms
    // // NOLINTEND(readability-magic-numbers, cppcoreguidelines-avoid-magic-numbers)

    // LOG_INF("double_endpoint: %lf", double_endpoint); // NOLINT
    // LOG_INF("integer_endpoint: %" PRIu32, integer_endpoint); // NOLINT
    // LOG_INF("boolean_endpoint: %d", boolean_endpoint); // NOLINT
    // LOG_INF("longinteger_endpoint: %lld", longinteger_endpoint); // NOLINT
    // LOG_INF("string_endpoint: %s", string_endpoint); // NOLINT
    // for (size_t i = 0; i < ARRAY_SIZE(binaryblob_endpoint); i++) {
    //     LOG_INF("binaryblob_endpoint element %i: %d", i, binaryblob_endpoint[i]); // NOLINT
    // }
    // LOG_INF("datetime_endpoint: %lld", datetime_endpoint); // NOLINT

    // // NOLINTBEGIN(readability-magic-numbers, cppcoreguidelines-avoid-magic-numbers)
    // double doublearray_endpoint[] = { 11.2, 2.2, 99.9, 421.1 };
    // int32_t integerarray_endpoint[] = { 11, 2, 21 };
    // bool booleanarray_endpoint[] = { true, false, true };
    // int64_t longintegerarray_endpoint[] = { 45993543534, 5 };
    // const char *stringarray_endpoint[] = { "hello", "world" };
    // uint8_t binaryblob_array_1[] = { 0x46, 0x23, 0x11 };
    // uint8_t binaryblob_array_2[] = { 0x43, 0x00, 0xEE };
    // size_t binaryblob_array_sizes[]
    //     = { ARRAY_SIZE(binaryblob_array_1), ARRAY_SIZE(binaryblob_array_2) };
    // uint8_t *binaryblobarray_endpoint[] = { binaryblob_array_1, binaryblob_array_2 };
    // int64_t datetimearray_endpoint[] = { 1710157717000 };
    // // NOLINTEND(readability-magic-numbers, cppcoreguidelines-avoid-magic-numbers)

    // for (size_t i = 0; i < ARRAY_SIZE(doublearray_endpoint); i++) {
    //     LOG_INF("doublearray_endpoint element %i: %f", i, doublearray_endpoint[i]); // NOLINT
    // }
    // for (size_t i = 0; i < ARRAY_SIZE(integerarray_endpoint); i++) {
    //     LOG_INF("integerarray_endpoint element %i: %d", i, integerarray_endpoint[i]); // NOLINT
    // }
    // for (size_t i = 0; i < ARRAY_SIZE(booleanarray_endpoint); i++) {
    //     LOG_INF("booleanarray_endpoint element %i: %d", i, booleanarray_endpoint[i]); // NOLINT
    // }
    // for (size_t i = 0; i < ARRAY_SIZE(longintegerarray_endpoint); i++) {
    //     // NOLINTNEXTLINE
    //     LOG_INF("longintegerarray_endpoint element %i: %lld", i, longintegerarray_endpoint[i]);
    // }
    // for (size_t i = 0; i < ARRAY_SIZE(stringarray_endpoint); i++) {
    //     LOG_INF("stringarray_endpoint element %i: %s", i, stringarray_endpoint[i]); // NOLINT
    // }
    // for (size_t i = 0; i < ARRAY_SIZE(binaryblob_array_1); i++) {
    //     LOG_INF("binaryblob_array_1 element %i: %d", i, binaryblob_array_1[i]); // NOLINT
    // }
    // for (size_t i = 0; i < ARRAY_SIZE(binaryblob_array_2); i++) {
    //     LOG_INF("binaryblob_array_2 element %i: %d", i, binaryblob_array_2[i]); // NOLINT
    // }
    // for (size_t i = 0; i < ARRAY_SIZE(datetimearray_endpoint); i++) {
    //     LOG_INF("datetimearray_endpoint element %i: %lld", i, datetimearray_endpoint[i]); //
    //     NOLINT
    // }

    // astarte_value_pair_t value_pairs[] = {
    //     { .endpoint = "double_endpoint", .value = astarte_value_from_double(double_endpoint) },
    //     { .endpoint = "integer_endpoint", .value = astarte_value_from_integer(integer_endpoint)
    //     }, { .endpoint = "boolean_endpoint", .value =
    //     astarte_value_from_boolean(boolean_endpoint) }, { .endpoint = "binaryblob_endpoint",
    //         .value
    //         = astarte_value_from_binaryblob(binaryblob_endpoint, ARRAY_SIZE(binaryblob_endpoint))
    //         },
    //     { .endpoint = "longinteger_endpoint",
    //         .value = astarte_value_from_longinteger(longinteger_endpoint) },
    //     { .endpoint = "string_endpoint", .value = astarte_value_from_string(string_endpoint) },
    //     { .endpoint = "datetime_endpoint",
    //         .value = astarte_value_from_datetime(datetime_endpoint) },
    //     { .endpoint = "doublearray_endpoint",
    //         .value = astarte_value_from_double_array(
    //             doublearray_endpoint, ARRAY_SIZE(doublearray_endpoint)) },
    //     { .endpoint = "integerarray_endpoint",
    //         .value = astarte_value_from_integer_array(
    //             integerarray_endpoint, ARRAY_SIZE(integerarray_endpoint)) },
    //     { .endpoint = "booleanarray_endpoint",
    //         .value = astarte_value_from_boolean_array(
    //             booleanarray_endpoint, ARRAY_SIZE(booleanarray_endpoint)) },
    //     { .endpoint = "longintegerarray_endpoint",
    //         .value = astarte_value_from_longinteger_array(
    //             longintegerarray_endpoint, ARRAY_SIZE(longintegerarray_endpoint)) },
    //     { .endpoint = "stringarray_endpoint",
    //         .value = astarte_value_from_string_array(
    //             stringarray_endpoint, ARRAY_SIZE(stringarray_endpoint)) },
    //     { .endpoint = "binaryblobarray_endpoint",
    //         .value = astarte_value_from_binaryblob_array((const void **)
    //         binaryblobarray_endpoint,
    //             binaryblob_array_sizes, ARRAY_SIZE(binaryblobarray_endpoint)) },
    //     { .endpoint = "datetimearray_endpoint",
    //         .value = astarte_value_from_datetime_array(
    //             datetimearray_endpoint, ARRAY_SIZE(datetimearray_endpoint)) },
    // };

    // astarte_result_t res = astarte_device_stream_aggregated(event.device,
    //     org_astarteplatform_zephyr_examples_DeviceAggregate.name, "/sensor24", value_pairs,
    //     ARRAY_SIZE(value_pairs), NULL, 0);
    // if (res != ASTARTE_RESULT_OK) {
    //     LOG_ERR("Error streaming the aggregate"); // NOLINT
    // }
}
