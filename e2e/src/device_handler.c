/*
 * (C) Copyright 2025, SECO Mind Srl
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "device_handler.h"

#include <zephyr/logging/log.h>
#include <zephyr/sys/atomic.h>

#include "astarte_device_sdk/device.h"
#include "utilities.h"
#include "zephyr/kernel.h"

#include "astarte_generated_interfaces.h"

LOG_MODULE_REGISTER(device_handler, CONFIG_DEVICE_HANDLER_LOG_LEVEL);

/************************************************
 *   Constants, static variables and defines    *
 ***********************************************/

#define GENERIC_WAIT_SLEEP_500_MS 500

static astarte_device_handle_t device_handle;

static const astarte_interface_t *device_interfaces[] = {
    &org_astarte_platform_zephyr_e2etest_DeviceAggregate,
    &org_astarte_platform_zephyr_e2etest_DeviceDatastream,
    &org_astarte_platform_zephyr_e2etest_DeviceProperty,
    &org_astarte_platform_zephyr_e2etest_ServerAggregate,
    &org_astarte_platform_zephyr_e2etest_ServerDatastream,
    &org_astarte_platform_zephyr_e2etest_ServerProperty,
};

K_THREAD_STACK_DEFINE(device_thread_stack_area, CONFIG_DEVICE_THREAD_STACK_SIZE);
static struct k_thread device_thread_data;
static atomic_t device_thread_flags;
enum device_thread_flags
{
    DEVICE_THREAD_CONNECTED_FLAG = 0,
    DEVICE_THREAD_TERMINATION_FLAG,
};

/************************************************
 *         Static functions declaration         *
 ***********************************************/

static void device_thread_entry_point(void *unused1, void *unused2, void *unused3);

static void connection_callback(astarte_device_connection_event_t event);
static void disconnection_callback(astarte_device_disconnection_event_t event);
static void device_individual_callback(astarte_device_datastream_individual_event_t event);
static void device_object_callback(astarte_device_datastream_object_event_t event);
static void device_property_set_callback(astarte_device_property_set_event_t event);
static void device_property_unset_callback(astarte_device_data_event_t event);

/************************************************
 *         Global functions definition          *
 ***********************************************/

void setup_device(void *data)
{
    LOG_INF("Creating static astarte_device.");

    CHECK_HALT(device_handle != NULL, "Attempting to create a device while a device is present.");

    astarte_device_config_t config = {
        .device_id = CONFIG_DEVICE_ID,
        .cred_secr = CONFIG_CREDENTIAL_SECRET,
        .interfaces = device_interfaces,
        .interfaces_size = ARRAY_SIZE(device_interfaces),
        .http_timeout_ms = CONFIG_HTTP_TIMEOUT_MS,
        .mqtt_connection_timeout_ms = CONFIG_MQTT_CONNECTION_TIMEOUT_MS,
        .mqtt_poll_timeout_ms = CONFIG_MQTT_POLL_TIMEOUT_MS,
        .cbk_user_data = data,
        .connection_cbk = connection_callback,
        .disconnection_cbk = disconnection_callback,
        .datastream_individual_cbk = device_individual_callback,
        .datastream_object_cbk = device_object_callback,
        .property_set_cbk = device_property_set_callback,
        .property_unset_cbk = device_property_unset_callback,
    };

    CHECK_ASTARTE_OK_HALT(astarte_device_new(&config, &device_handle), "Device creation failure.");

    LOG_INF("Spawning a new thread to poll data from the Astarte device.");
    k_thread_create(&device_thread_data, device_thread_stack_area,
        K_THREAD_STACK_SIZEOF(device_thread_stack_area), device_thread_entry_point, NULL, NULL,
        NULL, CONFIG_DEVICE_THREAD_PRIORITY, 0, K_NO_WAIT);

    LOG_INF("Astarte device created.");
}

void free_device()
{
    atomic_set_bit(&device_thread_flags, DEVICE_THREAD_TERMINATION_FLAG);

    CHECK_HALT(k_thread_join(&device_thread_data, K_FOREVER) != 0,
        "Failed in waiting for the Astarte thread to terminate.");

    LOG_INF("Destroing Astarte device and freeing resources.");
    CHECK_ASTARTE_OK_HALT(astarte_device_destroy(device_handle), "Device destruction failure.");
    device_handle = NULL;
    LOG_INF("Astarte device destroyed.");
}

astarte_device_handle_t get_device()
{
    CHECK_HALT(device_handle == NULL, "Trying to get non existent device.");
    return device_handle;
}

void wait_for_device_connection()
{
    CHECK_HALT(device_handle == NULL, "Trying to wait on a non existent device.");
    while (!atomic_test_bit(&device_thread_flags, DEVICE_THREAD_CONNECTED_FLAG)) {
        k_sleep(K_MSEC(GENERIC_WAIT_SLEEP_500_MS));
    }
}

void disconnect_device()
{
    CHECK_HALT(device_handle == NULL, "Trying to disconnect a non existent device.");
    atomic_set_bit(&device_thread_flags, DEVICE_THREAD_TERMINATION_FLAG);
}

void wait_for_device_disconnection()
{
    while (atomic_test_bit(&device_thread_flags, DEVICE_THREAD_CONNECTED_FLAG)) {
        k_sleep(K_MSEC(GENERIC_WAIT_SLEEP_500_MS));
    }
}

/************************************************
 *         Static functions definitions         *
 ***********************************************/

static void device_thread_entry_point(void *unused1, void *unused2, void *unused3)
{
    ARG_UNUSED(unused1);
    ARG_UNUSED(unused2);
    ARG_UNUSED(unused3);

    LOG_INF("Started Astarte device thread.");

    CHECK_ASTARTE_OK_HALT(astarte_device_connect(device_handle), "Device connection failure.");

    while (!atomic_test_bit(&device_thread_flags, DEVICE_THREAD_TERMINATION_FLAG)) {
        k_timepoint_t timepoint = sys_timepoint_calc(K_MSEC(CONFIG_DEVICE_POLL_PERIOD_MS));

        astarte_result_t res = astarte_device_poll(device_handle);
        CHECK_HALT(
            res != ASTARTE_RESULT_TIMEOUT && res != ASTARTE_RESULT_OK, "Device poll failure.");

        k_sleep(sys_timepoint_timeout(timepoint));
    }

    CHECK_ASTARTE_OK_HALT(
        astarte_device_disconnect(device_handle, K_SECONDS(10)), "Device disconnection failure.");

    LOG_INF("Exiting from the polling thread.");
}

static void connection_callback(astarte_device_connection_event_t event)
{
    (void) event;
    LOG_INF("Astarte device connected");
    atomic_set_bit(&device_thread_flags, DEVICE_THREAD_CONNECTED_FLAG);
}
static void disconnection_callback(astarte_device_disconnection_event_t event)
{
    (void) event;
    LOG_INF("Astarte device disconnected");
    atomic_clear_bit(&device_thread_flags, DEVICE_THREAD_CONNECTED_FLAG);
}
static void device_individual_callback(astarte_device_datastream_individual_event_t event) {}
static void device_object_callback(astarte_device_datastream_object_event_t event) {}
static void device_property_set_callback(astarte_device_property_set_event_t event) {}
static void device_property_unset_callback(astarte_device_data_event_t event) {}

// #define MAIN_THREAD_SLEEP_MS 500

// // static astarte_device_handle_t device_handle;
// K_SEM_DEFINE(device_sem, 1, 1);

// // LOG_MODULE_REGISTER(device_handler, CONFIG_DEVICE_HANDLER_LOG_LEVEL); // NOLINT

// static bool get_termination();
// // write flag DEVICE_THREAD_CONNECTED_FLAG
// static void set_connected();
// static void set_disconnected();
// // --

// void device_setup(astarte_device_config_t config)
// {
//     // // override with local callbacks
//     // config.connection_cbk = connection_callback;
//     // config.disconnection_cbk = disconnection_callback;

//     // astarte_device_handle_t temp_handle = NULL;
//     // CHECK_HALT(k_sem_count_get(&device_sem) == 0, "The device is already initialized");

//     // LOG_INF("Creating static astarte_device by calling astarte_device_new."); // NOLINT
//     // CHECK_ASTARTE_OK_HALT(
//     //     astarte_device_new(&config, &temp_handle), "Astarte device creation failure.");

//     // // we take the semaphore after initializing the device to make sure that errors from
//     // // astarte_device_new can be handled separately
//     // CHECK_HALT(k_sem_take(&device_sem, K_NO_WAIT) != 0,
//     //     "Could not take the semaphore, the device is already initialized");
//     // device_handle = temp_handle;

//     // LOG_INF("Spawning a new thread to poll data from the Astarte device."); // NOLINT
//     // k_thread_create(&device_thread_data, device_thread_stack_area,
//     //     K_THREAD_STACK_SIZEOF(device_thread_stack_area), device_thread_entry_point, NULL,
//     NULL,
//     //     NULL, CONFIG_DEVICE_THREAD_PRIORITY, 0, K_NO_WAIT);
// }

// // astarte_device_handle_t get_device()
// // {
// //     // CHECK_HALT(k_sem_count_get(&device_sem) > 0 || get_termination(),
// //     //     "The device is not initialized or is terminating");

// //     return device_handle;
// // }

void set_termination()
{
    // atomic_set_bit(&device_thread_flags, DEVICE_THREAD_TERMINATION_FLAG);
}

// void wait_for_connection()
// {
//     // while (!atomic_test_bit(&device_thread_flags, DEVICE_THREAD_CONNECTED_FLAG)) {
//     //     k_sleep(K_MSEC(MAIN_THREAD_SLEEP_MS));
//     // }
// }

// void wait_for_disconnection()
// {
//     // while (atomic_test_bit(&device_thread_flags, DEVICE_THREAD_CONNECTED_FLAG)) {
//     //     k_sleep(K_MSEC(MAIN_THREAD_SLEEP_MS));
//     // }
// }

// void free_device()
// {
//     set_termination();

//     CHECK_HALT(k_thread_join(&device_thread_data, K_FOREVER) != 0,
//         "Failed in waiting for the Astarte thread to terminate.");

//     LOG_INF("Destroing Astarte device and freeing resources."); // NOLINT
//     CHECK_ASTARTE_OK_HALT(
//         astarte_device_destroy(device_handle), "Astarte device destruction failure.");

//     LOG_INF("Astarte device destroyed."); // NOLINT

//     LOG_INF("Giving back the semaphore lock"); // NOLINT
//     // allow creating another device with `device_setup`
//     device_handle = NULL;
//     k_sem_give(&device_sem);
// }

// static bool get_termination()
// {
//     // return atomic_test_bit(&device_thread_flags, DEVICE_THREAD_TERMINATION_FLAG);
// }

// static void set_connected()
// {
//     // atomic_set_bit(&device_thread_flags, DEVICE_THREAD_CONNECTED_FLAG);
// }

// static void set_disconnected()
// {
//     // atomic_clear_bit(&device_thread_flags, DEVICE_THREAD_CONNECTED_FLAG);
// }

// static void connection_callback(astarte_device_connection_event_t event)
// {
//     (void) event;
//     LOG_INF("Astarte device connected"); // NOLINT
//     set_connected();
// }

// static void disconnection_callback(astarte_device_disconnection_event_t event)
// {
//     (void) event;
//     LOG_INF("Astarte device disconnected"); // NOLINT
//     set_disconnected();
// }

// static void device_thread_entry_point(void *unused1, void *unused2, void *unused3)
// {
//     ARG_UNUSED(unused1);
//     ARG_UNUSED(unused2);
//     ARG_UNUSED(unused3);

//     LOG_INF("Starting e2e device thread."); // NOLINT

//     CHECK_ASTARTE_OK_HALT(
//         astarte_device_connect(device_handle), "Astarte device connection failure.");

//     while (!get_termination()) {
//         k_timepoint_t timepoint = sys_timepoint_calc(K_MSEC(CONFIG_DEVICE_POLL_PERIOD_MS));

//         astarte_result_t res = astarte_device_poll(device_handle);
//         CHECK_HALT(res != ASTARTE_RESULT_TIMEOUT && res != ASTARTE_RESULT_OK,
//             "Astarte device poll failure.");

//         k_sleep(sys_timepoint_timeout(timepoint));
//     }

//     CHECK_ASTARTE_OK_HALT(astarte_device_disconnect(device_handle, K_SECONDS(10)),
//         "Astarte device disconnection failure.");

//     LOG_INF("Exiting from the polling thread."); // NOLINT
// }
