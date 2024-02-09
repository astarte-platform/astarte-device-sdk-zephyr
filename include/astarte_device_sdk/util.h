/*
 * (C) Copyright 2024, SECO Mind Srl
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 * @brief Utility macros and functions
 */

#ifndef ASTARTE_DEVICE_SDK_UTIL_H
#define ASTARTE_DEVICE_SDK_UTIL_H

/**
 * @defgroup util Util
 * @ingroup astarte_device_sdk
 * @{
 */

#define DEFINE_ARRAY(NAME, TYPE) \
typedef struct { \
	TYPE *buf; \
	size_t len; \
} NAME;

#define CHECK_RESULT(IDEN) \
if ((IDEN) != ASTARTE_OK) return (IDEN);

/**
 * @}
 */
#endif /* ASTARTE_DEVICE_SDK_UTIL_H */
