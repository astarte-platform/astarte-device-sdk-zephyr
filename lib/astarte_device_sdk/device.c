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
#include <zephyr/net/mqtt.h>
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

/* Buffers for MQTT client. */
#define MQTT_RX_TX_BUFFER_SIZE 256
static uint8_t mqtt_rx_buffer[MQTT_RX_TX_BUFFER_SIZE];
static uint8_t mqtt_tx_buffer[MQTT_RX_TX_BUFFER_SIZE];

static sec_tag_t sec_tag_list[] = {
    CONFIG_ASTARTE_DEVICE_SDK_CA_CERT_TAG,
    CONFIG_ASTARTE_DEVICE_SDK_CLIENT_CERT_TAG,
};

// /* MQTT connection status */
// static bool mqtt_connected = false;

/************************************************
 *       Callbacks declaration/definition       *
 ***********************************************/

// NOLINTNEXTLINE(readability-function-size, hicpp-function-size)
static void mqtt_evt_handler(struct mqtt_client *const client, const struct mqtt_evt *evt)
{
    switch (evt->type) {
        case MQTT_EVT_CONNACK:
            if (evt->result != 0) {
                LOG_ERR("MQTT connect failed %d\n", evt->result); // NOLINT
                break;
            }

            // mqtt_connected = true;
            LOG_INF("MQTT client connected!\n"); // NOLINT

            break;

        case MQTT_EVT_DISCONNECT:
            LOG_INF("MQTT client disconnected %d\n", evt->result); // NOLINT

            // mqtt_connected = false;

            break;

        case MQTT_EVT_PUBACK:
            if (evt->result != 0) {
                LOG_ERR("MQTT PUBACK error %d\n", evt->result); // NOLINT
                break;
            }

            LOG_INF("PUBACK packet id: %u\n", evt->param.puback.message_id); // NOLINT

            break;

        case MQTT_EVT_PUBREC:
            if (evt->result != 0) {
                LOG_ERR("MQTT PUBREC error %d\n", evt->result); // NOLINT
                break;
            }

            LOG_INF("PUBREC packet id: %u\n", evt->param.pubrec.message_id); // NOLINT

            const struct mqtt_pubrel_param rel_param
                = { .message_id = evt->param.pubrec.message_id };

            int err = mqtt_publish_qos2_release(client, &rel_param);
            if (err != 0) {
                LOG_ERR("Failed to send MQTT PUBREL: %d\n", err); // NOLINT
            }

            break;

        case MQTT_EVT_PUBCOMP:
            if (evt->result != 0) {
                LOG_ERR("MQTT PUBCOMP error %d\n", evt->result); // NOLINT
                break;
            }

            LOG_INF("PUBCOMP packet id: %u\\nn", evt->param.pubcomp.message_id); // NOLINT

            break;

        case MQTT_EVT_PINGRESP:
            LOG_INF("PINGRESP packet\n"); // NOLINT
            break;

        default:
            LOG_WRN("Unhandled MQTT event: %d\n", evt->type); // NOLINT
            break;
    }
}

/************************************************
 *         Static functions declaration         *
 ***********************************************/

/**
 * @brief Print content of addrinfo struct.
 *
 * @param[in] input_addinfo addrinfo struct to print.
 */
static void dump_addrinfo(const struct addrinfo *input_addinfo);

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

    LOG_WRN("Client certificate: '%s'", pem_crt_buf); // NOLINT

    int tls_rc = tls_credential_add(CONFIG_ASTARTE_DEVICE_SDK_CLIENT_CERT_TAG,
        TLS_CREDENTIAL_SERVER_CERTIFICATE, pem_crt_buf, strlen(pem_crt_buf) + 1);
    if (tls_rc != 0) {
        LOG_ERR("Failed adding client crt to credentials %d.", tls_rc); // NOLINT
        free(device->broker_hostname);
        free(device->broker_port);
        goto end;
    }

    LOG_WRN("Client key: '%s'", privkey_buf); // NOLINT

    tls_rc = tls_credential_add(CONFIG_ASTARTE_DEVICE_SDK_CLIENT_CERT_TAG,
        TLS_CREDENTIAL_PRIVATE_KEY, privkey_buf, strlen(privkey_buf) + 1);
    if (tls_rc != 0) {
        LOG_ERR("Failed adding client private key to credentials %d.", tls_rc); // NOLINT
        free(device->broker_hostname);
        free(device->broker_port);
        goto end;
    }

    device->mqtt_first_timeout_ms = cfg->mqtt_first_timeout_ms;

end:
    return res;
}

astarte_err_t astarte_device_connect(astarte_device_t *device)
{
    struct zsock_addrinfo *broker_addrinfo = NULL;
    struct zsock_addrinfo hints;
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    int sock_rc
        = zsock_getaddrinfo(device->broker_hostname, device->broker_port, &hints, &broker_addrinfo);
    if (sock_rc != 0) {
        LOG_ERR("Unable to resolve broker address %d", sock_rc); // NOLINT
        LOG_ERR("Errno: %s\n", strerror(errno)); // NOLINT
        return ASTARTE_ERR_SOCKET;
    }

    dump_addrinfo(broker_addrinfo);

    // MQTT client configuration
    mqtt_client_init(&device->mqtt_client);
    device->mqtt_client.broker = broker_addrinfo->ai_addr;
    device->mqtt_client.evt_cb = mqtt_evt_handler;
    device->mqtt_client.client_id.utf8 = (uint8_t *) "zephyr_mqtt_client";
    device->mqtt_client.client_id.size = sizeof("zephyr_mqtt_client") - 1;
    device->mqtt_client.password = NULL;
    device->mqtt_client.user_name = NULL;
    device->mqtt_client.protocol_version = MQTT_VERSION_3_1_1;
    device->mqtt_client.transport.type = MQTT_TRANSPORT_SECURE;
    // device->mqtt_client.transport.type = MQTT_TRANSPORT_NON_SECURE;

    // MQTT TLS configuration
    struct mqtt_sec_config *tls_config = &(device->mqtt_client.transport.tls.config);
    tls_config->peer_verify = TLS_PEER_VERIFY_REQUIRED;
    // tls_config->peer_verify = TLS_PEER_VERIFY_NONE;
    tls_config->cipher_list = NULL;
    tls_config->sec_tag_list = sec_tag_list;
    tls_config->sec_tag_count = ARRAY_SIZE(sec_tag_list);
    // tls_config->sec_tag_list = NULL;
    // tls_config->sec_tag_count = 0;
    tls_config->hostname = device->broker_hostname;

    // MQTT buffers configuration
    device->mqtt_client.rx_buf = mqtt_rx_buffer;
    device->mqtt_client.rx_buf_size = sizeof(mqtt_rx_buffer);
    device->mqtt_client.tx_buf = mqtt_tx_buffer;
    device->mqtt_client.tx_buf_size = sizeof(mqtt_tx_buffer);

    // Request connection to broker
    int mqtt_rc = mqtt_connect(&device->mqtt_client);
    if (mqtt_rc != 0) {
        LOG_ERR("MQTT connection error (%d)\n", mqtt_rc); // NOLINT
        return ASTARTE_ERR_MQTT;
    }

    return ASTARTE_OK;
}

astarte_err_t astarte_device_poll(astarte_device_t *device)
{
    // Poll the socket
    struct zsock_pollfd socket_fds[1];
    int socket_nfds = 1;
    socket_fds[0].fd = device->mqtt_client.transport.tls.sock;
    socket_fds[0].events = ZSOCK_POLLIN;
    int socket_rc = zsock_poll(socket_fds, socket_nfds, device->mqtt_first_timeout_ms);
    if (socket_rc < 0) {
        LOG_ERR("Poll error: %d", errno); // NOLINT
        return ASTARTE_ERR_SOCKET;
    }
    if (socket_rc == 0) {
        LOG_ERR("First poll following connection request timed out."); // NOLINT
        return ASTARTE_ERR_MQTT;
    }
    // Process the MQTT response
    int mqtt_rc = mqtt_input(&device->mqtt_client);
    if (mqtt_rc != 0) {
        LOG_ERR("MQTT input failed (%d)\n", mqtt_rc); // NOLINT
        return ASTARTE_ERR_MQTT;
    }
    return ASTARTE_OK;
}
/************************************************
 *         Static functions definitions         *
 ***********************************************/

// NOLINTBEGIN Just used for logging during development
static void dump_addrinfo(const struct addrinfo *input_addinfo)
{
    char dst[16] = { 0 };
    inet_ntop(
        AF_INET, &((struct sockaddr_in *) input_addinfo->ai_addr)->sin_addr, dst, sizeof(dst));
    LOG_INF("addrinfo @%p: ai_family=%d, ai_socktype=%d, ai_protocol=%d, "
            "sa_family=%d, sin_port=%x, ip_addr=%s",
        input_addinfo, input_addinfo->ai_family, input_addinfo->ai_socktype,
        input_addinfo->ai_protocol, input_addinfo->ai_addr->sa_family,
        ((struct sockaddr_in *) input_addinfo->ai_addr)->sin_port, dst);
}
// NOLINTEND
