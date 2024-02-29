/*
 * (C) Copyright 2024, SECO Mind Srl
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef CRYPTO_H
#define CRYPTO_H

/**
 * @file crypto.h
 * @brief Functions used to generate a CSR (certificate signing request) and private key.
 *
 * @note This module relies on MbedTLS functionality.
 */

#include "astarte_device_sdk/astarte.h"
#include "astarte_device_sdk/error.h"

/** @brief Private key buffer size */
#define ASTARTE_CRYPTO_PRIVKEY_BUFFER_SIZE 4096
/** @brief Certificate signing request buffer size */
#define ASTARTE_CRYPTO_CSR_BUFFER_SIZE 4096

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Create a private key to be used in a CSR.
 *
 * @param[out] privkey_pem Buffer where to store the computed private key, in the PEM format.
 * @param[in] privkey_pem_size Size of preallocated private key buffer.
 * @return ASTARTE_OK if successful, otherwise an error code.
 */
astarte_err_t astarte_crypto_create_key(unsigned char *privkey_pem, size_t privkey_pem_size);

/**
 * @brief Create a CSR (certificate signing request).
 *
 * @param[in] privkey_pem Private key to use for CSR generation, in PEM format.
 * @param[out] csr_pem Buffer where to store the computed CSR, in the PEM format.
 * @param[in] csr_pem_size Size of preallocated CSR buffer.
 * @return ASTARTE_OK if successful, otherwise an error code.
 */
astarte_err_t astarte_crypto_create_csr(
    const unsigned char *privkey_pem, unsigned char *csr_pem, size_t csr_pem_size);

/**
 * @brief Get the common name for a PEM certificate.
 *
 * @param[in] cert_pem Certificate in the PEM format.
 * @param[out] cert_cn Resulting common name.
 * @param[in] cert_cn_size Size of preallocated common name buffer/
 * @return ASTARTE_OK if successful, otherwise an error code.
 */
astarte_err_t astarte_crypto_get_certificate_common_name(
    const char *cert_pem, char *cert_cn, size_t cert_cn_size);

#ifdef __cplusplus
}
#endif

#endif /* CRYPTO_H */
