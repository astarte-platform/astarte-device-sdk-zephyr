/*
 * (C) Copyright 2024, SECO Mind Srl
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef ASTARTE_DEVICE_SDK_ASTARTE_CRYPTO_H
#define ASTARTE_DEVICE_SDK_ASTARTE_CRYPTO_H

/**
 * @file crypto.h
 * @brief Functions used to generate a CSR (certificate signing request) and private key.
 *
 * @note This module relies on MbedTLS functionality.
 */

#include "astarte_device_sdk/astarte.h"
#include "astarte_device_sdk/error.h"

#define ASTARTE_CRYPTO_PRIVKEY_BUFFER_SIZE 16000
#define ASTARTE_CRYPTO_CSR_BUFFER_SIZE 4096

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Create a private key to be used in a CSR.
 *
 * @param[out] privkey_buf Preallocated buffer where to store the computed private key.
 * @param[in] privkey_buf_size Size of preallocated output buffer.
 * @return ASTARTE_OK if successful, otherwise an error code.
 */
astarte_err_t astarte_crypto_create_key(unsigned char *privkey_buf, size_t privkey_buf_size);

/**
 * @brief Create a CSR (certificate signing request).
 *
 * @param[in] privkey Private key to use for CSR generation.
 * @param[out] csr_buf Preallocated buffer where to store the computed CSR.
 * @param[in] csr_buf_size Size of preallocated output buffer.
 * @return ASTARTE_OK if successful, otherwise an error code.
 */
astarte_err_t astarte_crypto_create_csr(
    const unsigned char *privkey, unsigned char *csr_buf, size_t csr_buf_size);

#ifdef __cplusplus
}
#endif

#endif /* ASTARTE_DEVICE_SDK_ASTARTE_CRYPTO_H */
