/*
 * (C) Copyright 2024, SECO Mind Srl
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef DATA_PRIVATE_H
#define DATA_PRIVATE_H

/**
 * @file data_private.h
 * @brief Private definitions for Astarte data types
 */
#include "astarte_device_sdk/data.h"

#include "astarte_device_sdk/astarte.h"
#include "astarte_device_sdk/interface.h"
#include "astarte_device_sdk/object.h"
#include "bson_deserializer.h"
#include "bson_serializer.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Appends the content of an #astarte_data_t to a BSON document.
 *
 * @param[in,out] bson a valid handle for the serializer instance.
 * @param[in] key BSON key name, which is a C string.
 * @param[in] data the #astarte_data_t to serialize to the bson.
 * @return ASTARTE_RESULT_OK if successful, otherwise an error code.
 */
astarte_result_t astarte_data_serialize(
    astarte_bson_serializer_t *bson, const char *key, astarte_data_t data);

/**
 * @brief Deserialize a BSON element to an #astarte_data_t.
 *
 * @warning This function might perform dynamic allocation, as such any data deserialized with
 * this function should be destroyed calling #astarte_data_destroy_deserialized.
 *
 * @note No encapsulation is permitted in the BSON element, the data should be directy accessible.
 *
 * @param[in] bson_elem The BSON element containing the data to deserialize.
 * @param[in] type An expected mapping type for the Astarte data.
 * @param[out] data The resulting data.
 * @return ASTARTE_RESULT_OK if successful, otherwise an error code.
 */
astarte_result_t astarte_data_deserialize(
    astarte_bson_element_t bson_elem, astarte_mapping_type_t type, astarte_data_t *data);

/**
 * @brief Destroy the data serialized with #astarte_data_deserialize.
 *
 * @warning This function will free all dynamically allocated memory allocated by
 * #astarte_data_deserialize. As so it should only be called on #astarte_data_t
 * that have been created with #astarte_data_deserialize.
 *
 * @note This function will have no effect on scalar #astarte_data_t, but only act on array
 * types.
 *
 * @param[in] data Data of which the content will be destroyed.
 */
void astarte_data_destroy_deserialized(astarte_data_t data);

#ifdef __cplusplus
}
#endif

#endif /* DATA_PRIVATE_H */
