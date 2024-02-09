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

#include <zephyr/net/mqtt.h>

#include "astarte_device_sdk/astarte.h"
#include "astarte_device_sdk/error.h"
#include "astarte_device_sdk/interface.h"
#include "astarte_device_sdk/pairing.h"

/** @brief Max allowed hostname characters are 253 */
#define ASTARTE_MAX_MQTT_BROKER_HOSTNAME_LEN 253
/** @brief Max allowed port number is 65535 */
#define ASTARTE_MAX_MQTT_BROKER_PORT_LEN 5

typedef struct astarte_device_t *astarte_handle_t;

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
    /** @brief Polling timeout for MQTT connection. */
    int32_t mqtt_connection_timeout_ms;
    /** @brief Polling timeout for MQTT operation. */
    int32_t mqtt_connected_timeout_ms;
    /** @brief Credential secret to be used for connecting to Astarte. */
    char cred_secr[ASTARTE_PAIRING_CRED_SECR_LEN + 1];
} astarte_device_config_t;

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize an Astarte device.
 *
 * @details This function has to be called to initialize the device SDK before doing anything else.
 * If an error code is returned the astarte_device_free function must not be called.
 *
 * @note A device can be instantiated and connected to Astarte only if it has been previously
 * registered on Astarte.
 *
 * @param[in] cfg Configuration struct.
 * @param[out] device Device instance to initialize.
 * @return result code of the initialization
 * @retval ASTARTE_OK successful initialization
 * @retval ASTARTE_ERR_INVALID_PARAM if the passed device_handle_ptr is NULL
 * @retval ASTARTE_ERR_OUT_OF_MEMORY could't allocate memory needed for the device handle
 * @retval ASTARTE_ERR_INTERFACE_ALREADY_PRESENT an interface in the cfg interfaces array is duplicated
 */
astarte_err_t astarte_device_init(astarte_device_config_t *cfg, astarte_handle_t *device_handle);

/**
 * @brief Frees an Astarte device initialized with #astarte_device_init.
 *
 * @details This function has to be called to free correctly the device SDK.
 *
 * @param[in,out] device A correctly initialized device handle.
 */
void astarte_device_free(astarte_handle_t device);

/**
 * @brief Connect a device to Astarte.
 *
 * @param[in] device Device instance to connect to Astarte.
 * @return ASTARTE_OK if successful, otherwise an error code.
 */
astarte_err_t astarte_device_connect(astarte_handle_t device);

/**
 * @brief Poll data from Astarte.
 *
 * @param[in] device Device instance to connect to Astarte.
 * @return ASTARTE_OK if successful, otherwise an error code.
 */
astarte_err_t astarte_device_poll(astarte_handle_t device);

#ifdef __cplusplus
}
#endif

/**
 * @}
 */

#endif /* ASTARTE_DEVICE_SDK_DEVICE_H */
