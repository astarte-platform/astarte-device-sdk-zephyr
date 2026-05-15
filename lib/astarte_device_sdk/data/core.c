/*
 * (C) Copyright 2026, SECO Mind Srl
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "astarte_device_sdk/data.h"

#include "log.h"

ASTARTE_LOG_MODULE_REGISTER(data, CONFIG_ASTARTE_DEVICE_SDK_DATA_LOG_LEVEL);

/************************************************
 *        Defines, constants and typedef        *
 ***********************************************/

// clang-format off
// NOLINTBEGIN(bugprone-macro-parentheses)
#define GENERATE_FROM_SCALAR(func_suffix, union_field, map_tag, val_type)                          \
    astarte_data_t astarte_data_from_##func_suffix(val_type val)                                   \
    {                                                                                              \
        return (astarte_data_t){                                                                   \
            .data = { .union_field = val },                                                        \
            .tag = (map_tag),                                                                      \
            .is_owned = false,                                                                     \
        };                                                                                         \
    }

#define GENERATE_TO_SCALAR(func_suffix, union_field, map_tag, val_type)                            \
    astarte_result_t astarte_data_to_##func_suffix(astarte_data_t data, val_type *val)             \
    {                                                                                              \
        if (!val || (data.tag != (map_tag))) {                                                     \
            ASTARTE_LOG_ERR("Conversion from Astarte data to %s error.", #func_suffix);            \
            return ASTARTE_RESULT_INVALID_PARAM;                                                   \
        }                                                                                          \
        *val = data.data.union_field;                                                              \
        return ASTARTE_RESULT_OK;                                                                  \
    }

#define GENERATE_FROM_ARRAY(func_suffix, union_field, map_tag, ptr_type)                           \
    astarte_data_t astarte_data_from_##func_suffix(ptr_type buf, size_t len)                       \
    {                                                                                              \
        return (astarte_data_t) {                                                                  \
            .data = {                                                                              \
                .union_field = { .buf = buf, .len = len },                                         \
            },                                                                                     \
            .tag = (map_tag),                                                                      \
            .is_owned = false,                                                                     \
        };                                                                                         \
    }

#define GENERATE_TO_ARRAY(func_suffix, union_field, map_tag, ptr_type)                             \
    astarte_result_t astarte_data_to_##func_suffix(                                                \
        astarte_data_t data, ptr_type *buf, size_t *len)                                           \
    {                                                                                              \
        if (!buf || !len || (data.tag != (map_tag))) {                                             \
            ASTARTE_LOG_ERR("Conversion from Astarte data to %s error.", #func_suffix);            \
            return ASTARTE_RESULT_INVALID_PARAM;                                                   \
        }                                                                                          \
        *buf = data.data.union_field.buf;                                                          \
        *len = data.data.union_field.len;                                                          \
        return ASTARTE_RESULT_OK;                                                                  \
    }
// NOLINTEND(bugprone-macro-parentheses)
// clang-format on

/************************************************
 *         Global functions definitions         *
 ***********************************************/

GENERATE_FROM_ARRAY(binaryblob, binaryblob, ASTARTE_MAPPING_TYPE_BINARYBLOB, const void *)
GENERATE_FROM_SCALAR(boolean, boolean, ASTARTE_MAPPING_TYPE_BOOLEAN, bool)
GENERATE_FROM_SCALAR(datetime, datetime, ASTARTE_MAPPING_TYPE_DATETIME, int64_t)
GENERATE_FROM_SCALAR(double, dbl, ASTARTE_MAPPING_TYPE_DOUBLE, double)
GENERATE_FROM_SCALAR(integer, integer, ASTARTE_MAPPING_TYPE_INTEGER, int32_t)
GENERATE_FROM_SCALAR(longinteger, longinteger, ASTARTE_MAPPING_TYPE_LONGINTEGER, int64_t)
GENERATE_FROM_SCALAR(string, string, ASTARTE_MAPPING_TYPE_STRING, const char *)

astarte_data_t astarte_data_from_binaryblob_array(const void **blobs, size_t *sizes, size_t count)
{
    return (astarte_data_t) {
        .data = {
            .binaryblob_array = {
                .blobs = blobs,
                .sizes = sizes,
                .count = count,
            },
        },
        .tag = ASTARTE_MAPPING_TYPE_BINARYBLOBARRAY,
        .is_owned = false,
    };
}

GENERATE_FROM_ARRAY(boolean_array, boolean_array, ASTARTE_MAPPING_TYPE_BOOLEANARRAY, const bool *)
GENERATE_FROM_ARRAY(
    datetime_array, datetime_array, ASTARTE_MAPPING_TYPE_DATETIMEARRAY, const int64_t *)
GENERATE_FROM_ARRAY(double_array, double_array, ASTARTE_MAPPING_TYPE_DOUBLEARRAY, const double *)
GENERATE_FROM_ARRAY(
    integer_array, integer_array, ASTARTE_MAPPING_TYPE_INTEGERARRAY, const int32_t *)
GENERATE_FROM_ARRAY(
    longinteger_array, longinteger_array, ASTARTE_MAPPING_TYPE_LONGINTEGERARRAY, const int64_t *)
GENERATE_FROM_ARRAY(string_array, string_array, ASTARTE_MAPPING_TYPE_STRINGARRAY, const char **)

astarte_mapping_type_t astarte_data_get_type(astarte_data_t data)
{
    return data.tag;
}

GENERATE_TO_ARRAY(binaryblob, binaryblob, ASTARTE_MAPPING_TYPE_BINARYBLOB, const void *)
GENERATE_TO_SCALAR(boolean, boolean, ASTARTE_MAPPING_TYPE_BOOLEAN, bool)
GENERATE_TO_SCALAR(datetime, datetime, ASTARTE_MAPPING_TYPE_DATETIME, int64_t)
GENERATE_TO_SCALAR(double, dbl, ASTARTE_MAPPING_TYPE_DOUBLE, double)
GENERATE_TO_SCALAR(integer, integer, ASTARTE_MAPPING_TYPE_INTEGER, int32_t)
GENERATE_TO_SCALAR(longinteger, longinteger, ASTARTE_MAPPING_TYPE_LONGINTEGER, int64_t)
GENERATE_TO_SCALAR(string, string, ASTARTE_MAPPING_TYPE_STRING, const char *)

astarte_result_t astarte_data_to_binaryblob_array(
    astarte_data_t data, const void ***blobs, size_t **sizes, size_t *count)
{
    if (!blobs || !sizes || !count || (data.tag != ASTARTE_MAPPING_TYPE_BINARYBLOBARRAY)) {
        ASTARTE_LOG_ERR("Conversion from Astarte data to binaryblob_array error.");
        return ASTARTE_RESULT_INVALID_PARAM;
    }
    *blobs = data.data.binaryblob_array.blobs;
    *sizes = data.data.binaryblob_array.sizes;
    *count = data.data.binaryblob_array.count;
    return ASTARTE_RESULT_OK;
}

GENERATE_TO_ARRAY(boolean_array, boolean_array, ASTARTE_MAPPING_TYPE_BOOLEANARRAY, const bool *)
GENERATE_TO_ARRAY(
    datetime_array, datetime_array, ASTARTE_MAPPING_TYPE_DATETIMEARRAY, const int64_t *)
GENERATE_TO_ARRAY(double_array, double_array, ASTARTE_MAPPING_TYPE_DOUBLEARRAY, const double *)
GENERATE_TO_ARRAY(integer_array, integer_array, ASTARTE_MAPPING_TYPE_INTEGERARRAY, const int32_t *)
GENERATE_TO_ARRAY(
    longinteger_array, longinteger_array, ASTARTE_MAPPING_TYPE_LONGINTEGERARRAY, const int64_t *)
GENERATE_TO_ARRAY(string_array, string_array, ASTARTE_MAPPING_TYPE_STRINGARRAY, const char **)
