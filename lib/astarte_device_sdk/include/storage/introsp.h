/*
 * (C) Copyright 2026, SECO Mind Srl
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef STORAGE_INTROSPECTION_H
#define STORAGE_INTROSPECTION_H

/**
 * @file storage/introsp.h
 * @brief Storage functions for the Astarte device introspection.
 */

#include "astarte_device_sdk/astarte.h"
#include "astarte_device_sdk/result.h"

#include "storage/core.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Store the introspection for this device.
 *
 * @param[in,out] handle Pointer to an initialized handle structure.
 * @param[in] intr Buffer containing the stringified version of the device introspection
 * @param[in] intr_size Size in chars of the @p buffer parameter.
 * @return ASTARTE_RESULT_OK if successful, otherwise an error code.
 */
astarte_result_t astarte_storage_introspection_store(
    astarte_storage_data_t *handle, const char *intr, size_t intr_size);

/**
 * @brief Check if the stored introspection exists and it's identical to the input one.
 *
 * @param[in,out] handle Pointer to an initialized handle structure.
 * @param[in] intr Buffer containing the stringified version of the device introspection
 * @param[in] intr_size Size in chars of the @p buffer parameter.
 * @return ASTARTE_RESULT_OK if successful, otherwise an error code.
 */
astarte_result_t astarte_storage_introspection_check(
    astarte_storage_data_t *handle, const char *intr, size_t intr_size);

#ifdef __cplusplus
}
#endif

#endif // STORAGE_INTROSPECTION_H
