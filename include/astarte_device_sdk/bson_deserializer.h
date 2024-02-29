/*
 * (C) Copyright 2024, SECO Mind Srl
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef ASTARTE_DEVICE_SDK_BSON_DESERIALIZER_H
#define ASTARTE_DEVICE_SDK_BSON_DESERIALIZER_H

/**
 * @file bson_deserializer.h
 * @brief Astarte BSON deserialization functions.
 *
 * @details This library follows the v1.1 of the BSON standard, but does not provide support
 * for the full specification, only for a smaller subset. For more information regarding the BSON
 * format specifications see: https://bsonspec.org/spec.html.
 *
 * @ingroup utils
 * @{
 */

#include "astarte_device_sdk/error.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/** @brief Bson document object */
typedef struct
{
    /** @brief Total size of the document in bytes */
    uint32_t size;
    /** @brief Pointer to the head of the list of elements in the BSON document */
    const void *list;
    /** @brief Size of the list in bytes */
    uint32_t list_size;
} bson_document_t;

/** @brief Bson element object */
typedef struct
{
    /** @brief Element type, see astarte_bson_types.h for the available types */
    uint8_t type;
    /** @brief String containing the element name */
    const char *name;
    /** @brief Length in bytes of the element name, not including the null terminator */
    size_t name_len;
    /** @brief Pointer to the element content */
    const void *value;
} bson_element_t;

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Perform some checks on the validity of the BSON.
 *
 * @note This function performs a very rough validation check. It is quite possible a malformed
 * bson file would still pass this check.
 *
 * @param[in] buffer Buffer containing the document to check.
 * @param[in] buffer_size Size of the allocated buffer containing the document.
 * @return True when BSON file is valid, false otherwise.
 */
bool bson_deserializer_check_validity(const void *buffer, size_t buffer_size);

/**
 * @brief Initialize a document type from a BSON data buffer.
 *
 * @param[in] buffer Buffer containing the BSON data.
 * @return Initialized document struct.
 */
bson_document_t bson_deserializer_init_doc(const void *buffer);

/**
 * @brief Get the first element in a document's list.
 *
 * @param[in] document Document from which to extract the element.
 * @param[out] element Used to store the extracted element.
 * @return ASTARTE_OK if successful, ASTARTE_ERR_NOT_FOUND if the document is empty.
 */
astarte_err_t bson_deserializer_first_element(bson_document_t document, bson_element_t *element);

/**
 * @brief Get the next element in a list.
 *
 * @param[in] document Document containing the list.
 * @param[in] curr_element Pointer to the current element.
 * @param[out] next_element Used to store the extracted element.
 * @return ASTARTE_OK if successful, ASTARTE_ERR_NOT_FOUND if no next element exists.
 */
astarte_err_t bson_deserializer_next_element(
    bson_document_t document, bson_element_t curr_element, bson_element_t *next_element);

/**
 * @brief Extract the value from the passed element.
 *
 * @param[in] element Element to extract the value from.
 * @return Extracted value.
 */
double bson_deserializer_element_to_double(bson_element_t element);

/**
 * @brief Extract the value from the passed element.
 *
 * @param[in] element Element to extract the value from.
 * @param[out] len Returned string length. Optional, pass NULL if not used.
 * @return Extracted value.
 */
const char *bson_deserializer_element_to_string(bson_element_t element, uint32_t *len);

/**
 * @brief Extract the value from the passed element.
 *
 * @param[in] element Element to extract the value from.
 * @return Extracted value.
 */
bson_document_t bson_deserializer_element_to_document(bson_element_t element);

/**
 * @brief Extract the value from the passed element.
 *
 * @note An array is encoded as a document, so a document type is returned.
 *
 * @param[in] element Element to extract the value from.
 * @return Extracted value.
 */
bson_document_t bson_deserializer_element_to_array(bson_element_t element);

/**
 * @brief Extract the value from the passed element.
 *
 * @param[in] element Element to extract the value from.
 * @param[out] len Returned bytes array length. Optional, pass NULL if not used.
 * @return Extracted value.
 */
const uint8_t *bson_deserializer_element_to_binary(bson_element_t element, uint32_t *len);

/**
 * @brief Extract the value from the passed element.
 *
 * @note The actual returned value is an uint8_t.
 *
 * @param[in] element Element to extract the value from.
 * @return Extracted value.
 */
bool bson_deserializer_element_to_bool(bson_element_t element);

/**
 * @brief Extract the value from the passed element.
 *
 * @note UTC datetime is encoded as a timestamp in an int64_t.
 *
 * @param[in] element Element to extract the value from.
 * @return Extracted value.
 */
int64_t bson_deserializer_element_to_datetime(bson_element_t element);

/**
 * @brief Extract the value from the passed element.
 *
 * @param[in] element Element to extract the value from.
 * @return Extracted value.
 */
int32_t bson_deserializer_element_to_int32(bson_element_t element);

/**
 * @brief Extract the value from the passed element.
 *
 * @param[in] element Element to extract the value from.
 * @return Extracted value.
 */
int64_t bson_deserializer_element_to_int64(bson_element_t element);

/**
 * @brief Fetch the element with name corresponding to the specified key from the document.
 *
 * @param[in] document Document to use for the search.
 * @param[in] key Element name to use as key for the lookup.
 * @param[out] element Used to return the found element, should be a pointer to an uninitialized
 * element.
 * @return ASTARTE_OK if successful, ASTARTE_ERR_NOT_FOUND if the element does not exist.
 */
astarte_err_t bson_deserializer_element_lookup(
    bson_document_t document, const char *key, bson_element_t *element);

#ifdef __cplusplus
}
#endif

/**
 * @}
 */

#endif // ASTARTE_DEVICE_SDK_BSON_DESERIALIZER_H
