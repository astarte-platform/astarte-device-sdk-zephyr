/*
 * (C) Copyright 2026, SECO Mind Srl
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef STORAGE_SYNCHRONIZATION_H
#define STORAGE_SYNCHRONIZATION_H

/**
 * @file storage/sync.h
 * @brief Storage functions for the Astarte device synchronization.
 */

#include "astarte_device_sdk/astarte.h"
#include "astarte_device_sdk/result.h"

#include "storage/core.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Get the synchronization state.
 *
 * @param[in,out] handle Pointer to an initialized handle structure.
 * @param[out] sync Synchronization state, this will be set to true if a proper synchronization
 * has been previously achieved with Astarte.
 * @return ASTARTE_RESULT_OK if successful, otherwise an error code.
 */
astarte_result_t astarte_storage_synchronization_get(astarte_storage_data_t *handle, bool *sync);

/**
 * @brief Set the synchronization state.
 *
 * @param[in,out] handle Pointer to an initialized handle structure.
 * @param[in] sync Synchronization state, this should be set to true if a proper synchronization
 * has been achieved with Astarte.
 * @return ASTARTE_RESULT_OK if successful, otherwise an error code.
 */
astarte_result_t astarte_storage_synchronization_set(astarte_storage_data_t *handle, bool sync);

#ifdef __cplusplus
}
#endif

#endif // STORAGE_SYNCHRONIZATION_H
