/*
 * (C) Copyright 2025, SECO Mind Srl
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "shell_handlers.h"

#include <zephyr/shell/shell.h>
#include <zephyr/sys/base64.h>

#include <astarte_device_sdk/device.h>
#include <astarte_device_sdk/interface.h>
#include <data_deserialize.h>
#include <interface_private.h>
#include <object_private.h>

#include "data.h"
#include "device_handler.h"
#include "idata.h"
#include "utilities.h"

LOG_MODULE_REGISTER(shell_handlers, CONFIG_SHELL_HANDLERS_LOG_LEVEL);

/************************************************
 *   Constants, static variables and defines    *
 ***********************************************/

#define DISCONNECT_CMD dvcshellcmd_disconnect
#define DISCONNECT_HELP "Disconnect the device and end the executable"

#define SEND_CMD dvcshellcmd_send
#define SEND_HELP "Send device data"

#define SEND_SUBCMD_SET send_subcommand_set

#define SEND_INDIVIDUAL_ARG individual
#define SEND_INDIVIDUAL_HELP                                                                       \
    "Send an individual property from the device with the data passed as argument. This command "  \
    "expects <interface_name> <path> <bson_value> <optional_timestamp>"

#define SEND_OBJECT_ARG object
#define SEND_OBJECT_HELP                                                                           \
    "Send an object from the device with the data passed as argument. This command expects "       \
    "<interface_name> <path> <bson_value> <optional_timestamp>"

#define SEND_PROPERTY_SUBCMD property
#define SEND_PROPERTY_HELP "Handle send of property interfaces subcommand."

#define SEND_PROPERTY_SUBCMD_SET send_property_subcommand_set

#define SEND_PROPERTY_SET_ARG set
#define SEND_PROPERTY_SET_HELP                                                                     \
    "Set a property with the data passed as argument. This command expects <interface_name> "      \
    "<path> <bson_value>"

#define SEND_PROPERTY_UNSET_ARG unset
#define SEND_PROPERTY_UNSET_HELP                                                                   \
    "Unset a property with the data passed as argument. This command expects <interface_name> "    \
    "<path>"

#define EXPECT_CMD dvcshellcmd_expect
#define EXPECT_HELP "Set the data expected from the server"

#define EXPECT_SUBCMD_SET expect_subcommand_set

#define EXPECT_INDIVIDUAL_ARG individual
#define EXPECT_INDIVIDUAL_HELP                                                                     \
    "Expect an individual property from the device with the data passed as argument. This "        \
    "command expects <interface_name> <path> <bson_value> <optional_timestamp>"

#define EXPECT_OBJECT_ARG object
#define EXPECT_OBJECT_HELP                                                                         \
    "Expect an object with the data passed as argument. This command expects <interface_name> "    \
    "<path> <bson_value> <optional_timestamp>"

#define EXPECT_PROPERTY_SUBCMD property
#define EXPECT_PROPERTY_HELP "Expect a property."

#define EXPECT_PROPERTY_SUBCMD_SET expect_property_subcommand_set

#define EXPECT_PROPERTY_SET_ARG set
#define EXPECT_PROPERTY_SET_HELP                                                                   \
    "Expect a property with the data passed as argument. This command expects <interface_name> "   \
    "<path> <bson_value>"

#define EXPECT_PROPERTY_UNSET_ARG unset
#define EXPECT_PROPERTY_UNSET_HELP                                                                 \
    "Expect an unset of the property with the data passed as argument. This command expects "      \
    "<interface_name> <path>"

static astarte_device_handle_t device;

// TODO: move this to the utils
typedef struct
{
    const uint8_t *buf;
    size_t len;
} byte_array_t;

// TODO: move this to the utils
typedef struct
{
    int64_t value;
    bool present;
} optional_value_t;

/************************************************
 *         Static functions declaration         *
 ***********************************************/

static int cmd_disconnect(const struct shell *sh, size_t argc, char **argv);

static int cmd_send_individual(const struct shell *shell, size_t argc, char **argv);
static int cmd_send_object(const struct shell *shell, size_t argc, char **argv);
static int cmd_send_property_set(const struct shell *shell, size_t argc, char **argv);
static int cmd_send_property_unset(const struct shell *shell, size_t argc, char **argv);

static int cmd_expect_individual(const struct shell *shell, size_t argc, char **argv);
static int cmd_expect_object(const struct shell *shell, size_t argc, char **argv);
static int cmd_expect_property_set(const struct shell *shell, size_t argc, char **argv);
static int cmd_expect_property_unset(const struct shell *shell, size_t argc, char **argv);

static void shell_helpers_skip_param(char ***args, size_t *argc);
static const astarte_interface_t *shell_helpers_next_interface_param(char ***args, size_t *argc);
static char *shell_helpers_next_alloc_string_param(char ***args, size_t *argc);
static byte_array_t shell_helpers_next_alloc_base64_param(char ***args, size_t *argc);
static optional_value_t shell_helpers_next_timestamp_param(char ***args, size_t *argc);

static int parse_bson_to_alloc_astarte_invividual(
    const astarte_interface_t *interface, char *path, byte_array_t *buf, astarte_data_t *out_data);
static int parse_bson_to_alloc_astarte_object(const astarte_interface_t *interface, char *path,
    byte_array_t *buf, astarte_object_entry_t **out_entries, size_t *out_entries_length);

/************************************************
 *          Shell commands declaration          *
 ***********************************************/

SHELL_CMD_REGISTER(DISCONNECT_CMD, NULL, DISCONNECT_HELP, cmd_disconnect);

SHELL_STATIC_SUBCMD_SET_CREATE(SEND_PROPERTY_SUBCMD_SET,
    SHELL_CMD_ARG(SEND_PROPERTY_SET_ARG, NULL, SEND_PROPERTY_SET_HELP, cmd_send_property_set, 4, 0),
    SHELL_CMD_ARG(
        SEND_PROPERTY_UNSET_ARG, NULL, SEND_PROPERTY_UNSET_HELP, cmd_send_property_unset, 3, 0),
    SHELL_SUBCMD_SET_END);
SHELL_STATIC_SUBCMD_SET_CREATE(SEND_SUBCMD_SET,
    SHELL_CMD_ARG(SEND_INDIVIDUAL_ARG, NULL, SEND_INDIVIDUAL_HELP, cmd_send_individual, 4, 1),
    SHELL_CMD_ARG(SEND_OBJECT_ARG, NULL, SEND_OBJECT_HELP, cmd_send_object, 4, 1),
    SHELL_CMD(SEND_PROPERTY_SUBCMD, &SEND_PROPERTY_SUBCMD_SET, SEND_PROPERTY_HELP, NULL),
    SHELL_SUBCMD_SET_END);
SHELL_CMD_REGISTER(SEND_CMD, &SEND_SUBCMD_SET, SEND_HELP, NULL);

SHELL_STATIC_SUBCMD_SET_CREATE(EXPECT_PROPERTY_SUBCMD_SET,
    SHELL_CMD_ARG(
        EXPECT_PROPERTY_SET_ARG, NULL, EXPECT_PROPERTY_SET_HELP, cmd_expect_property_set, 4, 0),
    SHELL_CMD_ARG(EXPECT_PROPERTY_UNSET_ARG, NULL, EXPECT_PROPERTY_UNSET_HELP,
        cmd_expect_property_unset, 3, 0),
    SHELL_SUBCMD_SET_END);
SHELL_STATIC_SUBCMD_SET_CREATE(EXPECT_SUBCMD_SET,
    SHELL_CMD_ARG(EXPECT_INDIVIDUAL_ARG, NULL, EXPECT_INDIVIDUAL_HELP, cmd_expect_individual, 4, 1),
    SHELL_CMD_ARG(EXPECT_OBJECT_ARG, NULL, EXPECT_OBJECT_HELP, cmd_expect_object, 4, 1),
    SHELL_CMD(EXPECT_PROPERTY_SUBCMD, &EXPECT_PROPERTY_SUBCMD_SET, EXPECT_PROPERTY_HELP, NULL),
    SHELL_SUBCMD_SET_END);
SHELL_CMD_REGISTER(EXPECT_CMD, &EXPECT_SUBCMD_SET, EXPECT_HELP, NULL);

/************************************************
 *         Global functions definition          *
 ***********************************************/

void init_shell(astarte_device_handle_t device_handle)
{
    device = device_handle;
}

/************************************************
 *         Static functions definitions         *
 ***********************************************/

static int cmd_disconnect(const struct shell *sh, size_t argc, char **argv)
{
    ARG_UNUSED(sh);
    ARG_UNUSED(argc);
    ARG_UNUSED(argv);

    LOG_INF("Disconnect command handler");
    LOG_INF("Stopping and joining the astarte device polling thread.");
    disconnect_device();
    return 0;
}

static int cmd_send_individual(const struct shell *shell, size_t argc, char **argv)
{
    ARG_UNUSED(shell);

    int return_code = 1;
    char *path = NULL;
    byte_array_t value = { 0 };
    astarte_data_t data = { 0 };

    LOG_INF("Send individual command handler");

    // First parameter is the command name (maybe?)
    shell_helpers_skip_param(&argv, &argc);

    // Second parameter is the interface name
    const astarte_interface_t *interface = shell_helpers_next_interface_param(&argv, &argc);
    CHECK_GOTO(!interface, cleanup, "Invalid interface name passed");

    // Third parameter is the path
    path = shell_helpers_next_alloc_string_param(&argv, &argc);
    CHECK_GOTO(!path, cleanup, "Invalid path parameter passed");

    // Fourth parameter is the payload
    value = shell_helpers_next_alloc_base64_param(&argv, &argc);
    CHECK_GOTO(value.len == 0, cleanup, "Invalid individual parameter passed");
    CHECK_GOTO(parse_bson_to_alloc_astarte_invividual(interface, path, &value, &data) != 0, cleanup,
        "Could not parse the BSON to an Astarte individual");

    // Fifth parameter is the timestamp
    optional_value_t timestamp = shell_helpers_next_timestamp_param(&argv, &argc);

    // Tansmit the payload using the Astarte device
    astarte_result_t res = ASTARTE_RESULT_OK;
    if (timestamp.present) {
        res = astarte_device_send_individual(device, interface->name, path, data, &timestamp.value);
    } else {
        res = astarte_device_send_individual(device, interface->name, path, data, NULL);
    }
    CHECK_ASTARTE_OK_GOTO(res, cleanup, "Failed to send individual to astarte");

    LOG_INF("Individual datastream sent");
    return_code = 0;

cleanup:
    data_destroy_deserialized(data);
    free((void *) value.buf);
    free(path);

    return return_code;
}

static int cmd_send_object(const struct shell *shell, size_t argc, char **argv)
{
    ARG_UNUSED(shell);

    int return_code = 1;
    char *path = NULL;
    byte_array_t value = { 0 };
    astarte_object_entry_t *entries = NULL;
    size_t entries_length = 0;

    LOG_INF("Send object command handler");

    // First parameter is the command name (maybe?)
    shell_helpers_skip_param(&argv, &argc);

    // Second parameter is the interface name
    const astarte_interface_t *interface = shell_helpers_next_interface_param(&argv, &argc);
    CHECK_GOTO(!interface, cleanup, "Invalid interface name passed");

    // Third parameter is the path
    path = shell_helpers_next_alloc_string_param(&argv, &argc);
    CHECK_GOTO(!path, cleanup, "Invalid path parameter passed");

    // Fourth parameter is the payload
    value = shell_helpers_next_alloc_base64_param(&argv, &argc);
    CHECK_GOTO(value.len == 0, cleanup, "Invalid object parameter passed");
    CHECK_GOTO(
        parse_bson_to_alloc_astarte_object(interface, path, &value, &entries, &entries_length) != 0,
        cleanup, "Could not parse the BSON to an Astarte object");

    // Fifth parameter is the timestamp
    optional_value_t timestamp = shell_helpers_next_timestamp_param(&argv, &argc);

    astarte_result_t res = ASTARTE_RESULT_OK;
    if (timestamp.present) {
        res = astarte_device_send_object(
            device, interface->name, path, entries, entries_length, &timestamp.value);
    } else {
        res = astarte_device_send_object(
            device, interface->name, path, entries, entries_length, NULL);
    }
    CHECK_ASTARTE_OK_GOTO(res, cleanup, "Failed to send object to astarte");

    LOG_INF("Object datastream sent");
    return_code = 0;

cleanup:
    astarte_object_entries_destroy_deserialized(entries, entries_length);
    free((void *) value.buf);
    free(path);

    return return_code;
}

static int cmd_send_property_set(const struct shell *shell, size_t argc, char **argv)
{
    ARG_UNUSED(shell);

    int return_code = 1;
    char *path = NULL;
    byte_array_t value = { 0 };
    astarte_data_t data = { 0 };

    LOG_INF("Send property set command handler");

    // First parameter is the command name (maybe?)
    shell_helpers_skip_param(&argv, &argc);

    // Second parameter is the interface name
    const astarte_interface_t *interface = shell_helpers_next_interface_param(&argv, &argc);
    CHECK_GOTO(!interface, cleanup, "Invalid interface name passed");

    // Third parameter is the path
    path = shell_helpers_next_alloc_string_param(&argv, &argc);
    CHECK_GOTO(!path, cleanup, "Invalid path parameter passed");

    // Fourth parameter is the payload
    value = shell_helpers_next_alloc_base64_param(&argv, &argc);
    CHECK_GOTO(value.len == 0, cleanup, "Invalid individual parameter passed");
    CHECK_GOTO(parse_bson_to_alloc_astarte_invividual(interface, path, &value, &data) != 0, cleanup,
        "Could not parse the BSON to an Astarte individual");

    // Tansmit the payload using the Astarte device
    astarte_result_t res = astarte_device_set_property(device, interface->name, path, data);
    CHECK_ASTARTE_OK_GOTO(res, cleanup, "Failed to send set property to astarte");

    LOG_INF("Property set");
    return_code = 0;

cleanup:
    data_destroy_deserialized(data);
    free((void *) value.buf);
    free(path);

    return return_code;
}

static int cmd_send_property_unset(const struct shell *shell, size_t argc, char **argv)
{
    ARG_UNUSED(shell);

    int return_code = 1;
    char *path = NULL;

    LOG_INF("Send property unset command handler");

    // First parameter is the command name (maybe?)
    shell_helpers_skip_param(&argv, &argc);

    // Second parameter is the interface name
    const astarte_interface_t *interface = shell_helpers_next_interface_param(&argv, &argc);
    CHECK_GOTO(!interface, cleanup, "Invalid interface name passed");

    // Third parameter is the path
    path = shell_helpers_next_alloc_string_param(&argv, &argc);
    CHECK_GOTO(!path, cleanup, "Invalid path parameter passed");

    astarte_result_t res = astarte_device_unset_property(device, interface->name, path);
    CHECK_ASTARTE_OK_GOTO(res, cleanup, "Failed to send unset property to astarte");

    LOG_INF("Property unset");
    return_code = 0;

cleanup:
    free(path);

    return return_code;
}

static int cmd_expect_individual(const struct shell *shell, size_t argc, char **argv)
{
    ARG_UNUSED(shell);
    ARG_UNUSED(argc);
    ARG_UNUSED(argv);

    LOG_INF("Expect individual command handler");
    // Do something

    return 0;
}

static int cmd_expect_object(const struct shell *shell, size_t argc, char **argv)
{
    ARG_UNUSED(shell);
    ARG_UNUSED(argc);
    ARG_UNUSED(argv);

    LOG_INF("Expect object command handler");
    // Do something

    return 0;
}

static int cmd_expect_property_set(const struct shell *shell, size_t argc, char **argv)
{
    ARG_UNUSED(shell);
    ARG_UNUSED(argc);
    ARG_UNUSED(argv);

    LOG_INF("Expect set property command handler");
    // Do something

    return 0;
}

static int cmd_expect_property_unset(const struct shell *shell, size_t argc, char **argv)
{
    ARG_UNUSED(shell);
    ARG_UNUSED(argc);
    ARG_UNUSED(argv);

    LOG_INF("Expect unset property command handler");
    // Do something

    return 0;
}

static void shell_helpers_skip_param(char ***args, size_t *argc)
{
    if (*argc < 1) {
        // no more arguments
        return;
    }

    *args += 1;
    *argc -= 1;
}

static const astarte_interface_t *shell_helpers_next_interface_param(char ***args, size_t *argc)
{
    if (*argc < 1) {
        // no more arguments
        return NULL;
    }

    const char *const interface_name = (*args)[0];
    const astarte_interface_t *interface = data_get_interface(
        interface_name, strlen(interface_name));

    if (!interface) {
        // Interface not found
        LOG_ERR("Invalid interface name %s", interface_name);
        return NULL;
    }

    // move to the next parameter for caller
    *args += 1;
    *argc -= 1;
    return interface;
}

static char *shell_helpers_next_alloc_string_param(char ***args, size_t *argc)
{
    if (*argc < 1) {
        // no more arguments
        return NULL;
    }

    const char *const arg = (*args)[0];

    size_t arg_len = strlen(arg);
    char *const copied_arg = calloc(arg_len + 1, sizeof(char));
    CHECK_HALT(!copied_arg, "Could not allocate memory to copy string parameter");
    memcpy(copied_arg, arg, arg_len + 1);

    // move to the next parameter for caller
    *args += 1;
    *argc -= 1;
    return (char *) copied_arg;
}

static byte_array_t shell_helpers_next_alloc_base64_param(char ***args, size_t *argc)
{
    uint8_t *byte_array = NULL;

    if (*argc < 1) {
        // no more arguments
        goto error;
    }

    const char *const arg = (*args)[0];
    const size_t arg_len = strlen(arg);

    size_t byte_array_length = 0;
    int res = base64_decode(NULL, 0, &byte_array_length, arg, arg_len);
    if (byte_array_length == 0) {
        LOG_ERR("Error while computing base64 decode buffer length: %d", res);
        goto error;
    }

    LOG_DBG("The size of the decoded buffer is: %d", byte_array_length);

    byte_array = calloc(byte_array_length, sizeof(uint8_t));
    CHECK_HALT(!byte_array, "Out of memory");

    res = base64_decode(byte_array, byte_array_length, &byte_array_length, arg, arg_len);
    if (res != 0) {
        LOG_ERR("Error while decoding base64 argument %d", res);
        goto error;
    }

    // move to the next parameter for caller
    *args += 1;
    *argc -= 1;
    return (byte_array_t){ .buf = byte_array, .len = byte_array_length };

error:
    free(byte_array);
    return (byte_array_t){};
}

static optional_value_t shell_helpers_next_timestamp_param(char ***args, size_t *argc)
{
    const int base = 10;

    if (*argc < 1) {
        // no more arguments
        return (optional_value_t){};
    }

    const char *const arg = (*args)[0];
    const int64_t timestamp = (int64_t) strtoll(arg, NULL, base);

    // move to the next parameter for caller
    *args += 1;
    *argc -= 1;
    return (optional_value_t){
        .value = timestamp,
        .present = true,
    };
}

static int parse_bson_to_alloc_astarte_invividual(
    const astarte_interface_t *interface, char *path, byte_array_t *buf, astarte_data_t *out_data)
{
    const astarte_mapping_t *mapping = NULL;
    astarte_result_t res = astarte_interface_get_mapping_from_path(interface, path, &mapping);
    CHECK_ASTARTE_OK_RET_1(
        res, "Error while searching for the mapping (%d) %s", res, astarte_result_to_name(res));

    CHECK_RET_1(!astarte_bson_deserializer_check_validity(buf->buf, buf->len),
        "Invalid BSON document in data");
    astarte_bson_document_t full_document = astarte_bson_deserializer_init_doc(buf->buf);
    astarte_bson_element_t v_elem = { 0 };
    CHECK_ASTARTE_OK_RET_1(astarte_bson_deserializer_element_lookup(full_document, "v", &v_elem),
        "Cannot retrieve BSON value from data");

    CHECK_ASTARTE_OK_RET_1(data_deserialize(v_elem, mapping->type, out_data),
        "Couldn't deserialize received binary data into object entries");

    return 0;
}

static int parse_bson_to_alloc_astarte_object(const astarte_interface_t *interface, char *path,
    byte_array_t *buf, astarte_object_entry_t **out_entries, size_t *out_entries_length)
{
    CHECK_ASTARTE_OK_RET_1(!astarte_bson_deserializer_check_validity(buf->buf, buf->len),
        "Invalid BSON document in data");
    astarte_bson_document_t full_document = astarte_bson_deserializer_init_doc(buf->buf);
    astarte_bson_element_t v_elem = { 0 };
    CHECK_ASTARTE_OK_RET_1(astarte_bson_deserializer_element_lookup(full_document, "v", &v_elem),
        "Cannot retrieve BSON value from data");

    CHECK_ASTARTE_OK_RET_1(astarte_object_entries_deserialize(
                               v_elem, interface, path, out_entries, out_entries_length),
        "Couldn't deserialize received binary data into object entries");

    return 0;
}

// // NOLINTNEXTLINE
// static idata_handle_t idata;

// static int parse_alloc_astarte_invividual(const astarte_interface_t *interface, char *path,
//     idata_byte_array *buf, astarte_data_t *out_data);
// static int parse_alloc_astarte_object(const astarte_interface_t *interface, char *path,
//     idata_byte_array *buf, astarte_object_entry_t **entries, size_t *entries_length);

// static const astarte_interface_t *next_interface_parameter(char ***args, size_t *argc);
// static void skip_parameter(char ***args, size_t *argc);
// static idata_timestamp_option_t next_timestamp_parameter(char ***args, size_t *argc);
// // the return of this function needs to be deallocated
// static char *next_alloc_string_parameter(char ***args, size_t *argc);
// // the return of this function needs to be deallocated
// static idata_byte_array next_alloc_base64_parameter(char ***args, size_t *argc);

// // start expect commands handler
// int cmd_expect_individual_handler(const struct shell *sh, size_t argc, char **argv)
// {
//     LOG_INF("Expect individual command handler"); // NOLINT

//     char *path = NULL;
//     idata_byte_array individual_value = { 0 };
//     astarte_data_t data = { 0 };
//     // the first parameter is the command name
//     skip_parameter(&argv, &argc);
//     const astarte_interface_t *interface = next_interface_parameter(&argv, &argc);
//     CHECK_GOTO(!interface, cleanup, "Invalid interface name passed");
//     path = next_alloc_string_parameter(&argv, &argc);
//     CHECK_GOTO(!path, cleanup, "Invalid path parameter passed");
//     individual_value = next_alloc_base64_parameter(&argv, &argc);
//     CHECK_GOTO(individual_value.len == 0, cleanup, "Invalid individual parameter passed");
//     idata_timestamp_option_t timestamp = next_timestamp_parameter(&argv, &argc);

//     CHECK_GOTO(parse_alloc_astarte_invividual(interface, path, &individual_value, &data) != 0,
//         cleanup, "Could not parse and allocate astarte individual");

//     // path and individual will be freed by the idata_unit free function
//     CHECK_GOTO(idata_add_individual(idata, interface,
//                    (idata_individual_t){
//                        .data = data,
//                        .path = path,
//                        .timestamp = timestamp,
//                    })
//             != 0,
//         cleanup, "Could not insert individual in expected_data");

//     // should get freed even if no errors occur because it is not stored anywhere and it's not
//     // needed
//     free((void *) individual_value.buf);
//     return 0;

// cleanup:
//     free(path);
//     free((void *) individual_value.buf);
//     data_destroy_deserialized(data);
//     return 1;
// }

// int cmd_expect_object_handler(const struct shell *sh, size_t argc, char **argv)
// {
//     LOG_INF("Expect object command handler"); // NOLINT

//     char *path = NULL;
//     idata_byte_array object_bytes = { 0 };
//     astarte_object_entry_t *entries = { 0 };
//     size_t entries_length = { 0 };
//     skip_parameter(&argv, &argc);
//     const astarte_interface_t *interface = next_interface_parameter(&argv, &argc);
//     CHECK_GOTO(!interface, cleanup, "Invalid interface name passed");
//     path = next_alloc_string_parameter(&argv, &argc);
//     CHECK_GOTO(!path, cleanup, "Invalid path parameter passed");
//     object_bytes = next_alloc_base64_parameter(&argv, &argc);
//     CHECK_GOTO(object_bytes.len == 0, cleanup, "Invalid object parameter passed");
//     idata_timestamp_option_t timestamp = next_timestamp_parameter(&argv, &argc);

//     CHECK_GOTO(
//         parse_alloc_astarte_object(interface, path, &object_bytes, &entries, &entries_length) !=
//         0, cleanup, "Could not parse and allocate astarte object entries");

//     // path, object_bytes and object_entries will be freed by the idata_unit free function
//     CHECK_GOTO(idata_add_object(idata, interface, (idata_object_t) {
//              .entries = {
//                  .buf = entries,
//                  .len = entries_length,
//              },
//             .path = path,
//             // do not free in case of objects since entries keys are pointing to this buffer
//             .object_bytes = object_bytes,
//             .timestamp = timestamp,
//          }) != 0, cleanup, "Could not add object entry to idata list");

//     return 0;

// cleanup:
//     free(path);
//     free((void *) object_bytes.buf);
//     astarte_object_entries_destroy_deserialized(entries, entries_length);
//     return 1;
// }

// int cmd_expect_property_set_handler(const struct shell *sh, size_t argc, char **argv)
// {
//     LOG_INF("Expect set property command handler"); // NOLINT

//     char *path = NULL;
//     idata_byte_array property_value = { 0 };
//     astarte_data_t data = { 0 };

//     skip_parameter(&argv, &argc);
//     const astarte_interface_t *interface = next_interface_parameter(&argv, &argc);
//     CHECK_GOTO(!interface, cleanup, "Invalid interface name passed");
//     path = next_alloc_string_parameter(&argv, &argc);
//     CHECK_GOTO(!path, cleanup, "Invalid path parameter passed");
//     property_value = next_alloc_base64_parameter(&argv, &argc);
//     CHECK_GOTO(property_value.len == 0, cleanup, "Invalid data parameter passed");

//     CHECK_GOTO(parse_alloc_astarte_invividual(interface, path, &property_value, &data) != 0,
//         cleanup, "Could not deserialize and allocate astarte data");

//     // path and data will be freed by the idata_unit free function
//     CHECK_GOTO(idata_add_property(idata, interface,
//                    (idata_property_t){
//                        .data = data,
//                        .path = path,
//                    })
//             != 0,
//         cleanup, "Could not add property to idata list");

//     // should get freed even if no errors occur because it is not stored anywhere and it's not
//     // needed
//     free((void *) property_value.buf);
//     return 0;

// cleanup:
//     free(path);
//     free((void *) property_value.buf);
//     data_destroy_deserialized(data);
//     return 1;
// }

// int cmd_expect_property_unset_handler(const struct shell *sh, size_t argc, char **argv)
// {
//     LOG_INF("Expect unset property command handler"); // NOLINT

//     char *path = NULL;
//     skip_parameter(&argv, &argc);
//     const astarte_interface_t *interface = next_interface_parameter(&argv, &argc);
//     CHECK_GOTO(!interface, cleanup, "Invalid interface name passed");
//     path = next_alloc_string_parameter(&argv, &argc);
//     CHECK_GOTO(!path, cleanup, "Invalid path parameter passed");

//     CHECK_GOTO(idata_add_property(idata, interface,
//                    (idata_property_t){
//                        .path = path,
//                        .unset = true,
//                    })
//             != 0,
//         cleanup, "Could not add property to idata list");

//     return 0;

// cleanup:
//     free(path);
//     return 1;
// }

// // start send commands handlers
// int cmd_send_individual_handler(const struct shell *sh, size_t argc, char **argv)
// {
//     LOG_INF("Send individual command handler"); // NOLINT

//     int return_code = 1;

//     char *path = NULL;
//     idata_byte_array individual_value = { 0 };
//     astarte_data_t data = { 0 };
//     skip_parameter(&argv, &argc);
//     const astarte_interface_t *interface = next_interface_parameter(&argv, &argc);
//     CHECK_GOTO(!interface, cleanup, "Invalid interface name passed");
//     path = next_alloc_string_parameter(&argv, &argc);
//     CHECK_GOTO(!path, cleanup, "Invalid path parameter passed");
//     individual_value = next_alloc_base64_parameter(&argv, &argc);
//     CHECK_GOTO(individual_value.len == 0, cleanup, "Invalid individual parameter passed");
//     idata_timestamp_option_t timestamp = next_timestamp_parameter(&argv, &argc);

//     CHECK_GOTO(parse_alloc_astarte_invividual(interface, path, &individual_value, &data) != 0,
//         cleanup, "Could not parse and allocate astarte individual");

//     astarte_result_t res = { 0 };
//     if (timestamp.present) {
//         res = astarte_device_send_individual(device, interface->name, path, data,
//         &timestamp.value);
//     } else {
//         res = astarte_device_send_individual(device, interface->name, path, data, NULL);
//     }
//     CHECK_ASTARTE_OK_GOTO(res, cleanup, "Failed to send individual to astarte");

//     shell_print(sh, "Sent individual");
//     return_code = 0;

// cleanup:
//     data_destroy_deserialized(data);
//     free((void *) individual_value.buf);
//     free(path);

//     return return_code;
// }

// int cmd_send_object_handler(const struct shell *sh, size_t argc, char **argv)
// {
//     LOG_INF("Send object command handler"); // NOLINT

//     int return_code = 1;

//     char *path = NULL;
//     idata_byte_array object_bytes = { 0 };
//     astarte_object_entry_t *entries = { 0 };
//     size_t entries_length = { 0 };
//     skip_parameter(&argv, &argc);
//     const astarte_interface_t *interface = next_interface_parameter(&argv, &argc);
//     CHECK_GOTO(!interface, cleanup, "Invalid interface name passed");
//     path = next_alloc_string_parameter(&argv, &argc);
//     CHECK_GOTO(!path, cleanup, "Invalid path parameter passed");
//     object_bytes = next_alloc_base64_parameter(&argv, &argc);
//     CHECK_GOTO(object_bytes.len == 0, cleanup, "Invalid object parameter passed");
//     idata_timestamp_option_t timestamp = next_timestamp_parameter(&argv, &argc);

//     CHECK_GOTO(
//         parse_alloc_astarte_object(interface, path, &object_bytes, &entries, &entries_length) !=
//         0, cleanup, "Could not parse and allocate astarte object entries");

//     astarte_result_t res = { 0 };
//     if (timestamp.present) {
//         res = astarte_device_send_object(
//             device, interface->name, path, entries, entries_length, &timestamp.value);
//     } else {
//         res = astarte_device_send_object(
//             device, interface->name, path, entries, entries_length, NULL);
//     }
//     CHECK_ASTARTE_OK_GOTO(res, cleanup, "Failed to send object to astarte");

//     shell_print(sh, "Sent object");
//     return_code = 0;

// cleanup:
//     astarte_object_entries_destroy_deserialized(entries, entries_length);
//     free((void *) object_bytes.buf);
//     free(path);

//     return return_code;
// }

// int cmd_send_property_set_handler(const struct shell *sh, size_t argc, char **argv)
// {
//     LOG_INF("Set property command handler"); // NOLINT

//     int return_code = 1;

//     char *path = NULL;
//     idata_byte_array property_value = { 0 };
//     astarte_data_t data = { 0 };
//     skip_parameter(&argv, &argc);
//     const astarte_interface_t *interface = next_interface_parameter(&argv, &argc);
//     CHECK_GOTO(!interface, cleanup, "Invalid interface name passed");
//     path = next_alloc_string_parameter(&argv, &argc);
//     CHECK_GOTO(!path, cleanup, "Invalid path parameter passed");
//     property_value = next_alloc_base64_parameter(&argv, &argc);
//     CHECK_GOTO(property_value.len == 0, cleanup, "Invalid data parameter passed");

//     CHECK_GOTO(parse_alloc_astarte_invividual(interface, path, &property_value, &data) != 0,
//         cleanup, "Could not parse and allocate data");

//     astarte_result_t res = astarte_device_set_property(device, interface->name, path, data);
//     CHECK_ASTARTE_OK_GOTO(res, cleanup, "Failed to send set property to astarte");

//     shell_print(sh, "Property set");
//     return_code = 0;

// cleanup:
//     data_destroy_deserialized(data);
//     free((void *) property_value.buf);
//     free(path);

//     return return_code;
// }

// int cmd_send_property_unset_handler(const struct shell *sh, size_t argc, char **argv)
// {
//     LOG_INF("Unset property command handler"); // NOLINT

//     int return_code = 1;

//     char *path = NULL;
//     skip_parameter(&argv, &argc);
//     const astarte_interface_t *interface = next_interface_parameter(&argv, &argc);
//     CHECK_GOTO(!interface, cleanup, "Invalid interface name passed");
//     path = next_alloc_string_parameter(&argv, &argc);
//     CHECK_GOTO(!path, cleanup, "Invalid path parameter passed");

//     astarte_result_t res = astarte_device_unset_property(device, interface->name, path);
//     CHECK_ASTARTE_OK_GOTO(res, cleanup, "Failed to send set property to astarte");

//     shell_print(sh, "Property unset");
//     return_code = 0;

// cleanup:
//     free(path);

//     return return_code;
// }

// static const astarte_interface_t *next_interface_parameter(char ***args, size_t *argc)
// {
//     if (*argc < 1) {
//         // no more arguments
//         return NULL;
//     }

//     const char *const interface_name = (*args)[0];
//     const astarte_interface_t *interface = idata_get_interface(
//         idata, interface_name, strlen(interface_name));

//     if (!interface) {
//         // no interface with name specified found
//         LOG_ERR("Invalid interface name %s", interface_name); // NOLINT
//         return NULL;
//     }

//     // move to the next parameter for caller
//     *args += 1;
//     *argc -= 1;
//     return interface;
// }

// static char *next_alloc_string_parameter(char ***args, size_t *argc)
// {
//     if (*argc < 1) {
//         // no more arguments
//         return NULL;
//     }

//     const char *const arg = (*args)[0];

//     size_t arg_len = strlen(arg);
//     char *const copied_arg = calloc(arg_len + 1, sizeof(char));
//     CHECK_HALT(!copied_arg, "Could not copy string parameter");
//     memcpy(copied_arg, arg, arg_len + 1);

//     // move to the next parameter for caller
//     *args += 1;
//     *argc -= 1;
//     return (char *) copied_arg;
// }

// static idata_byte_array next_alloc_base64_parameter(char ***args, size_t *argc)
// {
//     if (*argc < 1) {
//         // no more arguments
//         return (idata_byte_array){};
//     }

//     const char *const arg = (*args)[0];
//     const size_t arg_len = strlen(arg);

//     size_t byte_array_length = 0;
//     int res = base64_decode(NULL, 0, &byte_array_length, arg, arg_len);
//     if (byte_array_length == 0) {
//         LOG_ERR("Error while computing base64 decode buffer length: %d", res); // NOLINT
//         return (idata_byte_array){};
//     }

//     LOG_DBG("The size of the decoded buffer is: %d", byte_array_length); // NOLINT

//     uint8_t *const byte_array = calloc(byte_array_length, sizeof(uint8_t));
//     CHECK_HALT(!byte_array, "Out of memory");

//     res = base64_decode(byte_array, byte_array_length, &byte_array_length, arg, arg_len);
//     if (res != 0) {
//         LOG_ERR("Error while decoding base64 argument %d", res); // NOLINT
//         return (idata_byte_array){};
//     }

//     // move to the next parameter for caller
//     *args += 1;
//     *argc -= 1;
//     return (idata_byte_array){
//         .buf = byte_array,
//         .len = byte_array_length,
//     };
// }

// static idata_timestamp_option_t next_timestamp_parameter(char ***args, size_t *argc)
// {
//     const int base = 10;

//     if (*argc < 1) {
//         // no more arguments
//         return (idata_timestamp_option_t){};
//     }

//     const char *const arg = (*args)[0];
//     const int64_t timestamp = (int64_t) strtoll(arg, NULL, base);

//     // move to the next parameter for caller
//     *args += 1;
//     *argc -= 1;
//     return (idata_timestamp_option_t){
//         .value = timestamp,
//         .present = true,
//     };
// }

// // this also implicitly checks that the passed path is valid
// static int parse_alloc_astarte_invividual(const astarte_interface_t *interface, char *path,
//     idata_byte_array *buf, astarte_data_t *out_data)
// {
//     const astarte_mapping_t *mapping = NULL;
//     astarte_result_t res = astarte_interface_get_mapping_from_path(interface, path, &mapping);
//     CHECK_ASTARTE_OK_RET_1(
//         res, "Error while searching for the mapping (%d) %s", res, astarte_result_to_name(res));

//     CHECK_RET_1(!astarte_bson_deserializer_check_validity(buf->buf, buf->len),
//         "Invalid BSON document in data");
//     astarte_bson_document_t full_document = astarte_bson_deserializer_init_doc(buf->buf);
//     astarte_bson_element_t v_elem = { 0 };
//     CHECK_ASTARTE_OK_RET_1(astarte_bson_deserializer_element_lookup(full_document, "v", &v_elem),
//         "Cannot retrieve BSON value from data");

//     CHECK_ASTARTE_OK_RET_1(data_deserialize(v_elem, mapping->type, out_data),
//         "Couldn't deserialize received binary data into object entries");

//     return 0;
// }

// // this also implicitly checks that the passed path is valid
// static int parse_alloc_astarte_object(const astarte_interface_t *interface, char *path,
//     idata_byte_array *buf, astarte_object_entry_t **out_entries, size_t *out_entries_length)
// {
//     // Since the function expects a bson element we need to receive a "v" value like it would be
//     // sent to astarte
//     CHECK_RET_1(!astarte_bson_deserializer_check_validity(buf->buf, buf->len),
//         "Invalid BSON document in data");
//     astarte_bson_document_t full_document = astarte_bson_deserializer_init_doc(buf->buf);
//     astarte_bson_element_t v_elem = { 0 };
//     CHECK_ASTARTE_OK_RET_1(astarte_bson_deserializer_element_lookup(full_document, "v", &v_elem),
//         "Cannot retrieve BSON value from data");

//     CHECK_ASTARTE_OK_RET_1(astarte_object_entries_deserialize(
//                                v_elem, interface, path, out_entries, out_entries_length),
//         "Couldn't deserialize received binary data into object entries");

//     return 0;
// }
