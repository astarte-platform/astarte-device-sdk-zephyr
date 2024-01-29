/*
 * (C) Copyright 2024, SECO Mind Srl
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef ASTARTE_DEVICE_SDK_DEVICE_H
#define ASTARTE_DEVICE_SDK_DEVICE_H

/**
 * @file device.h
 * @brief Device management
 */

/**
 * @defgroup device Device management
 * @ingroup astarte_device_sdk
 * @{
 */

#include "astarte_device_sdk/astarte.h"
#include "astarte_device_sdk/error.h"
#include "astarte_device_sdk/pairing.h"

/**
 * @brief Configuration struct for an Astarte device.
 *
 * @details This configuration struct might be used to create a new instance of a device
 * using the init function.
 */
typedef struct
{
    /** @brief Timeout for HTTP requests. */
    int32_t http_timeout_ms;
    /** @brief Credential secret to be used for connection to Astarte. */
    char cred_secr[ASTARTE_PAIRING_CRED_SECR_LEN + 1];
} astarte_device_config_t;

/**
 * @brief Internal struct for an instance of an Astarte device.
 *
 * @warning Users should not modify the content of this struct directly
 */
typedef struct
{
    char *broker_hostname; /**< Internal field. */
    char *broker_port; /**< Internal field. */
} astarte_device_t;

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize an Astarte device.
 *
 * @details This function has to be called to initialize the device SDK before doing anything else.
 *
 * @note A device can be instantiated and connected to Astarte only if it has been previously
 * registered on Astarte.
 *
 * @param[in] cfg Configuration struct.
 * @param[out] device Device instance to initialize.
 * @return ASTARTE_OK if successful, otherwise an error code.
 */
astarte_err_t astarte_device_init(astarte_device_config_t *cfg, astarte_device_t *device);

#ifdef __cplusplus
}
#endif

/**
 * @}
 */

#endif /* ASTARTE_DEVICE_SDK_DEVICE_H */
