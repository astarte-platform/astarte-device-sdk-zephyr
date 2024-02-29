/*
 * (C) Copyright 2024, SECO Mind Srl
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file bson_types.h
 * @brief Astarte BSON types definitions.
 *
 * @ingroup utils
 * @{
 */

#ifndef ASTARTE_DEVICE_SDK_BSON_TYPES_H
#define ASTARTE_DEVICE_SDK_BSON_TYPES_H

// clang-format off

/** @brief Single byte value for the BSON type double */
#define BSON_TYPE_DOUBLE    '\x01'
/** @brief Single byte value for the BSON type string */
#define BSON_TYPE_STRING    '\x02'
/** @brief Single byte value for the BSON type document */
#define BSON_TYPE_DOCUMENT  '\x03'
/** @brief Single byte value for the BSON type array */
#define BSON_TYPE_ARRAY     '\x04'
/** @brief Single byte value for the BSON type binary */
#define BSON_TYPE_BINARY    '\x05'
/** @brief Single byte value for the BSON type boolean */
#define BSON_TYPE_BOOLEAN   '\x08'
/** @brief Single byte value for the BSON type datetime */
#define BSON_TYPE_DATETIME  '\x09'
/** @brief Single byte value for the BSON type 32 bits integer */
#define BSON_TYPE_INT32     '\x10'
/** @brief Single byte value for the BSON type 64 bits integer */
#define BSON_TYPE_INT64     '\x12'

/** @brief Single byte value for the BSON subtype generic binary (used in the binary type) */
#define BSON_SUBTYPE_DEFAULT_BINARY '\0'

// clang-format on

/**
 * @}
 */

#endif // ASTARTE_DEVICE_SDK_BSON_TYPES_H
