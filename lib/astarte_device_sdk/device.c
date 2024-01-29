/*
 * (C) Copyright 2024, SECO Mind Srl
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "astarte_device_sdk/device.h"

#include "astarte_crypto.h"
#include "astarte_device_sdk/pairing.h"
#include "astarte_pairing.h"

#include <stdlib.h>
#include <string.h>

#include <zephyr/kernel.h>
#include <zephyr/net/socket.h>
#include <zephyr/net/tls_credentials.h>

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(astarte_device, CONFIG_ASTARTE_DEVICE_SDK_DEVICE_LOG_LEVEL); // NOLINT

/************************************************
 *       Checks over configuration values       *
 ***********************************************/

/************************************************
 *        Defines, constants and typedef        *
 ***********************************************/
/************************************************
 *       Callbacks declaration/definition       *
 ***********************************************/

/************************************************
 *         Static functions declaration         *
 ***********************************************/

/************************************************
 *         Global functions definitions         *
 ***********************************************/

astarte_err_t astarte_device_init(astarte_device_config_t *cfg, astarte_device_t *device)
{
    astarte_err_t res = ASTARTE_OK;
    char broker_url[ASTARTE_PAIRING_MAX_BROKER_URL_LEN + 1];

    res = astarte_pairing_get_broker_url(
        cfg->http_timeout_ms, cfg->cred_secr, broker_url, ASTARTE_PAIRING_MAX_BROKER_URL_LEN + 1);
    if (res != ASTARTE_OK) {
        LOG_ERR("Failed in obtaining the MQTT broker URL"); // NOLINT
        goto end;
    }

    int strncmp_rc = strncmp(broker_url, "mqtts://", strlen("mqtts://"));
    if (strncmp_rc != 0) {
        LOG_ERR("MQTT broker URL is malformed"); // NOLINT
        res = ASTARTE_ERR_HTTP_REQUEST;
        goto end;
    }
    char *token = strtok(&broker_url[strlen("mqtts://")], ":");
    if (!token) {
        LOG_ERR("MQTT broker URL is malformed"); // NOLINT
        res = ASTARTE_ERR_HTTP_REQUEST;
        goto end;
    }
    const size_t broker_hostname_len = strlen(token);
    device->broker_hostname = calloc(1, broker_hostname_len + 1);
    if (!device->broker_hostname) {
        LOG_ERR("Out of memory %s: %d", __FILE__, __LINE__); // NOLINT
        res = ASTARTE_ERR_OUT_OF_MEMORY;
        goto end;
    }
    strncpy(device->broker_hostname, token, broker_hostname_len + 1);
    token = strtok(NULL, "/");
    if (!token) {
        LOG_ERR("MQTT broker URL is malformed"); // NOLINT
        res = ASTARTE_ERR_HTTP_REQUEST;
        free(device->broker_hostname);
        goto end;
    }
    const size_t broker_port_len = strlen(token);
    device->broker_port = calloc(1, broker_port_len + 1);
    if (!device->broker_port) {
        LOG_ERR("Out of memory %s: %d", __FILE__, __LINE__); // NOLINT
        res = ASTARTE_ERR_OUT_OF_MEMORY;
        free(device->broker_hostname);
        goto end;
    }
    strncpy(device->broker_port, token, broker_port_len + 1);

    LOG_WRN("Broker HOSTNAME: '%s', PORT: '%s'", device->broker_hostname,
        device->broker_port); // NOLINT

    unsigned char privkey_buf[ASTARTE_CRYPTO_PRIVKEY_BUFFER_SIZE];
    unsigned char pem_crt_buf[ASTARTE_PAIRING_MAX_CLIENT_CRT_LEN + 1];
    res = astarte_pairing_get_client_certificate(cfg->http_timeout_ms, cfg->cred_secr, privkey_buf,
        sizeof(privkey_buf), pem_crt_buf, sizeof(pem_crt_buf));
    if (res != ASTARTE_OK) {
        LOG_ERR("Failed in fetching the client certificate."); // NOLINT
        free(device->broker_hostname);
        free(device->broker_port);
        goto end;
    }

end:
    return res;
}
/************************************************
 *         Static functions definitions         *
 ***********************************************/
