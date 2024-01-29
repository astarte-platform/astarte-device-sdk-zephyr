/*
 * (C) Copyright 2024, SECO Mind Srl
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef ASTARTE_DEVICE_SDK_ASTARTE_PAIRING_H
#define ASTARTE_DEVICE_SDK_ASTARTE_PAIRING_H

/**
 * @file astarte_pairing.h
 * @brief Private pairing APIs for new Astarte devices
 */

#include "astarte_device_sdk/astarte.h"
#include "astarte_device_sdk/error.h"

/** @brief Maximum length for the MQTT broker URL in chars.
 *
 * @details It is the sum of:
 * - Size of the string `mqtts://` (8)
 * - Maximum length for a hostname (DNS) (253)
 * - Size of the string `:` (1)
 * - Maximum length for a port number (5)
 * - Size of the string `/` (1)
 *
 * Total: 268 characters
 */
#define ASTARTE_PAIRING_MAX_BROKER_URL_LEN 268

/** @brief Maximum supported size for a client certificate. */
// TODO make this an SDK configuration parameter, so users can select an appropriate value
// depending on their server configuration.
#define ASTARTE_PAIRING_MAX_CLIENT_CRT_LEN 4069

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Fetch the MQTT broker URL from Astarte.
 *
 * @param[in] timeout_ms Timeout to use for the HTTP operations in ms.
 * @param[in] cred_secr Credential secret to use as authorization token.
 * @param[out] out_url Output buffer where to store the fetched ULR.
 * @param[in] out_url_size Size of the output buffer for the URL.
 * @return ASTARTE_OK if successful, otherwise an error code.
 */
astarte_err_t astarte_pairing_get_broker_url(
    int32_t timeout_ms, const char *cred_secr, char *out_url, size_t out_url_size);

/**
 * @brief Fetch the client x509 certificate from Astarte.
 *
 * @warning This is often a very memory intensive operation, more than 20kB of stack are required.
 *
 * @param[in] timeout_ms Timeout to use for the HTTP operations in ms.
 * @param[in] cred_secr Credential secret to use as authorization token.
 * @param[out] privkey_buf Preallocated buffer where to store the computed private key.
 * @param[in] privkey_buf_size Size of preallocated output buffer.
 * @param[out] pem_crt Output buffer where to store the fetched ULR.
 * @param[in] pem_crt_size Size of the output buffer for the URL.
 * @return ASTARTE_OK if successful, otherwise an error code.
 */
astarte_err_t astarte_pairing_get_client_certificate(int32_t timeout_ms, const char *cred_secr,
    unsigned char *privkey_buf, size_t privkey_buf_size, char *pem_crt, size_t pem_crt_size);

#ifdef __cplusplus
}
#endif

#endif /* ASTARTE_DEVICE_SDK_ASTARTE_PAIRING_H */
