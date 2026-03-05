/*
 * (C) Copyright 2024, SECO Mind Srl
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "runner.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <zephyr/fatal.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/shell/shell.h>
#include <zephyr/shell/shell_uart.h>
#include <zephyr/sys/bitarray.h>
#include <zephyr/sys/util.h>

#include <astarte_device_sdk/data.h>
#include <astarte_device_sdk/device.h>
#include <astarte_device_sdk/interface.h>
#include <astarte_device_sdk/mapping.h>
#include <astarte_device_sdk/object.h>
#include <astarte_device_sdk/pairing.h>
#include <astarte_device_sdk/result.h>
#include <astarte_device_sdk/util.h>

#include "device_handler.h"
#include "idata.h"
#include "shell_handlers.h"
#include "shell_macros.h"
#include "utilities.h"

#include "astarte_generated_interfaces.h"

LOG_MODULE_REGISTER(runner, CONFIG_RUNNER_LOG_LEVEL);

/************************************************
 * Constants, static variables and defines
 ***********************************************/

// static const astarte_interface_t *interfaces[] = {
//     &org_astarte_platform_zephyr_e2etest_DeviceAggregate,
//     &org_astarte_platform_zephyr_e2etest_DeviceDatastream,
//     &org_astarte_platform_zephyr_e2etest_DeviceProperty,
//     &org_astarte_platform_zephyr_e2etest_ServerAggregate,
//     &org_astarte_platform_zephyr_e2etest_ServerDatastream,
//     &org_astarte_platform_zephyr_e2etest_ServerProperty,
// };

/************************************************
 * Static functions declaration
 ***********************************************/

// // device callbacks declaration
// static void device_individual_callback(astarte_device_datastream_individual_event_t event);
// static void device_object_callback(astarte_device_datastream_object_event_t event);
// static void device_property_set_callback(astarte_device_property_set_event_t event);
// static void device_property_unset_callback(astarte_device_data_event_t event);
// // --
// // checks that the interface data is empty and no more messages are expected
// static size_t check_idata_size(idata_handle_t idata);
// /**
//  * This function generates a unique key from a interface name.
//  */
// // NOTE change this function if interfaces name, or set of interfaces, change. It relies
// // on current interface names to create a simple but unique hash.
// // If more interfaces are added this function should also change.
// static uint64_t interfaces_perfect_hash(const char *key_string, size_t len);

/************************************************
 * Global functions definition
 ***********************************************/

void run_end_to_end_test()
{
    LOG_INF("End to end test runner");

#if CONFIG_LOG_ONLY
    LOG_WRN("Running with device callbacks in log only mode");
    LOG_WRN("Data received will NOT be checked against expected data");
#endif

    LOG_INF("Starting the device");
    // TODO: Replace this to a data structure that can temporarely store the expected data
    void *data = NULL;
    setup_device(data);

    // TODO: Avoid this passing around of the device handle completely
    init_shell(get_device());

    // Wait for the device connection
    LOG_INF("Waiting for the device to be connected");
    wait_for_device_connection();

    // We are ready to send and receive data
    const struct shell *uart_shell = shell_backend_uart_get_ptr();
    shell_start(uart_shell);

    // Pytest detects the readyness of the shell through this string
    shell_print(uart_shell, SHELL_IS_READY);

    // Wait untill a shell command disconnects the device
    wait_for_device_disconnection();
    // TODO: check that all devices expected messages have been received
    // CHECK_HALT(check_idata_size(idata) > 0, "Some expected messages didn't get received");

    // Pytest detects the a termination of the test through this string
    shell_print(uart_shell, SHELL_IS_CLOSING);
    shell_stop(uart_shell);

    // Free the device and epxected data structures after `shell_stop`
    free_device();
    // idata_free(idata);

#if CONFIG_LOG_ONLY
    LOG_WRN("Test has been run with device callbacks in log only mode");
    LOG_WRN("Data received didn't get checked against expected data");
#endif
}

/************************************************
 * Static functions definitions
 ***********************************************/

// // device data callbacks that check received data against expected
// static void device_individual_callback(astarte_device_datastream_individual_event_t event)
// {
//     LOG_INF("Individual datastream callback");
//     idata_handle_t idata = event.base_event.user_data;
//     const astarte_interface_t *interface = idata_get_interface(
//         idata, event.base_event.interface_name, strlen(event.base_event.interface_name));
//     CHECK_HALT(
//         interface == NULL, "The interface name received as event does not match any interface");

// #if !CONFIG_LOG_ONLY
//     idata_individual_t expected = { 0 };
//     CHECK_HALT(idata_pop_individual(idata, interface, &expected) != 0, "No more expected data");

//     CHECK_HALT(strcmp(expected.path, event.base_event.path) != 0,
//         "Received path does not match expected one");
//     CHECK_HALT(!astarte_data_equal(&expected.data, &event.data), "Unexpected element received");

//     free_individual(expected);
//     LOG_INF("Individual received matched expected one");
// #else
//     LOG_INF("Individual received on %s%s", interface->name, event.base_event.path);
//     utils_log_astarte_data(event.data);
// #endif
// }

// static void device_object_callback(astarte_device_datastream_object_event_t event)
// {
//     LOG_INF("Object datastream callback");

//     idata_handle_t idata = event.base_event.user_data;
//     idata_object_entry_array received = {
//         .buf = event.entries,
//         .len = event.entries_len,
//     };
//     const astarte_interface_t *interface = idata_get_interface(
//         idata, event.base_event.interface_name, strlen(event.base_event.interface_name));
//     CHECK_HALT(
//         interface == NULL, "The interface name received as event does not match any interface");

// #if !CONFIG_LOG_ONLY
//     idata_object_t expected = {};
//     CHECK_HALT(idata_pop_object(idata, interface, &expected) != 0, "No more expected data");

//     CHECK_HALT(strcmp(expected.path, event.base_event.path) != 0,
//         "Received path does not match expected one");
//     CHECK_HALT(!astarte_object_equal(&expected.entries, &received), "Unexpected element
//     received");

//     free_object(expected);
//     LOG_INF("Object received matched expected one");
// #else
//     LOG_INF("Aggregate data received on %s%s", interface->name, event.base_event.path);
//     utils_log_object_entry_array(&received);
// #endif
// }

// static void device_property_set_callback(astarte_device_property_set_event_t event)
// {
//     LOG_INF("Property set callback");

//     idata_handle_t idata = event.base_event.user_data;
//     const astarte_interface_t *interface = idata_get_interface(
//         idata, event.base_event.interface_name, strlen(event.base_event.interface_name));
//     CHECK_HALT(
//         interface == NULL, "The interface name received as event does not match any interface");

// #if !CONFIG_LOG_ONLY
//     idata_property_t expected = { 0 };
//     // inizialization
//     CHECK_HALT(idata_pop_property(idata, interface, &expected), "No more expected data");

//     CHECK_HALT(strcmp(expected.path, event.base_event.path) != 0,
//         "Received path does not match expected one");
//     CHECK_HALT(!astarte_data_equal(&expected.data, &event.data), "Unexpected element received");

//     free_property(expected);
//     LOG_INF("Property received matched expected one");
// #else
//     LOG_INF("Individual property set received on %s%s", interface->name, event.base_event.path);
//     utils_log_astarte_data(event.data);
// #endif
// }

// static void device_property_unset_callback(astarte_device_data_event_t event)
// {
//     LOG_INF("Property unset callback");

//     idata_handle_t idata = event.user_data;
//     const astarte_interface_t *interface = idata_get_interface(
//         idata, event.interface_name, strlen(event.interface_name));
//     CHECK_HALT(
//         interface == NULL, "The interface name received as event does not match any interface");

// #if !CONFIG_LOG_ONLY
//     idata_property_t expected = { 0 };
//     // inizialization
//     CHECK_HALT(idata_pop_property(idata, interface, &expected), "No more expected data");

//     CHECK_HALT(strcmp(expected.path, event.path) != 0, "Received path does not match expected
//     one"); CHECK_HALT(!expected.unset, "Unexpected unset received");

//     free_property(expected);
//     LOG_INF("Expected property unset received");
// #else
//     LOG_INF("Individual property unset received on %s%s", interface->name, event.path);
// #endif
// }

// static uint64_t interfaces_perfect_hash(const char *key_string, size_t len)
// {
//     const char interface_dname[] = "org.astarte-platform.zephyr.e2etest.";
//     const size_t interface_dname_ownership_idetifier = 36;
//     const size_t interface_dname_type_idetifier = 43;

//     // check that the string is a known interface name and has enough characters for our check
//     // NOTE this depends completely on the names used in this case this works because we have
//     names
//     // - ServerProperty
//     // - DeviceProperty
//     // - ServerAggregate
//     // - DeviceAggregate
//     // - ServerDatastream
//     // - DeviceDatastream
//     // hence the names are uniquely identified by the first letter and the seventh after the
//     // reverse domain notation base of the interfaces (`interfaace_dname`)
//     CHECK_HALT(strstr(key_string, interface_dname) != key_string || len <= 44,
//         "Received an invalid or unexpected interface name, please update the hash function");

//     uint32_t result = { 0 };
//     uint8_t *result_bytes = (uint8_t *) &result;
//     result_bytes[0] = key_string[interface_dname_ownership_idetifier];
//     result_bytes[1] = key_string[interface_dname_ownership_idetifier];
//     result_bytes[2] = key_string[interface_dname_type_idetifier];
//     result_bytes[3] = key_string[interface_dname_type_idetifier];

//     return result;
// }

// static size_t check_idata_size(idata_handle_t idata)
// {
//     size_t not_received_count = 0;
//     LOG_INF("Checking remaining expected messagges");

//     for (size_t i = 0; i < ARRAY_SIZE(interfaces); ++i) {
//         const astarte_interface_t *interface = interfaces[i];
//         not_received_count += idata_get_count(idata, interface);

//         if (interface->type == ASTARTE_INTERFACE_TYPE_PROPERTIES) {
//             idata_property_t *property = {};
//             if (idata_peek_property(idata, interface, &property) == 0) {
//                 LOG_WRN("A property was not received");
//                 utils_log_e2e_property(property);
//             }
//         } else if (interface->aggregation == ASTARTE_INTERFACE_AGGREGATION_OBJECT) {
//             idata_object_t *object = {};
//             if (idata_peek_object(idata, interface, &object) == 0) {
//                 LOG_WRN("An object was not received");
//                 utils_log_e2e_object(object);
//             }
//         } else if (interface->aggregation == ASTARTE_INTERFACE_AGGREGATION_INDIVIDUAL) {
//             idata_individual_t *individual = {};
//             if (idata_peek_individual(idata, interface, &individual) == 0) {
//                 LOG_WRN("An individual was not received");
//                 utils_log_e2e_individual(individual);
//             }
//         }
//     }

//     LOG_INF("Count of idata elements: %zu", not_received_count);

//     return not_received_count;
// }
