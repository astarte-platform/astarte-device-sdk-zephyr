/*
 * (C) Copyright 2024, SECO Mind Srl
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/data/json.h>
#include <zephyr/kernel.h>
#include <zephyr/net/http/client.h>
#include <zephyr/net/socket.h>
#include <zephyr/posix/arpa/inet.h>

#if !defined(CONFIG_ASTARTE_DEVICE_SDK_DEVELOP_DISABLE_TLS)
#include "ca_certificates.h"
#include <zephyr/net/tls_credentials.h>
#endif

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(main, CONFIG_APP_LOG_LEVEL); // NOLINT

#include <astarte_device_sdk/device.h>
#include <astarte_device_sdk/pairing.h>
#include <astarte_device_sdk/unit_testable_component.h>

#include "wifi.h"

int main(void)
{
    LOG_INF("MQTT Example\nBoard: %s", CONFIG_BOARD); // NOLINT

#if !defined(CONFIG_BOARD_NATIVE_SIM)
    LOG_INF("Initializing WiFi driver."); // NOLINT
    wifi_init();
#endif

    k_sleep(K_SECONDS(5)); // sleep for 5 seconds

#if !defined(CONFIG_ASTARTE_DEVICE_SDK_DEVELOP_DISABLE_TLS)
    tls_credential_add(CA_CERTIFICATE_ROOT_TAG, TLS_CREDENTIAL_CA_CERTIFICATE, ca_certificate_root,
        sizeof(ca_certificate_root));
#endif

    astarte_device_config_t device_config;
    device_config.http_timeout_ms = 3 * MSEC_PER_SEC;

    astarte_err_t astarte_err = astarte_pairing_register_device(
        device_config.http_timeout_ms, device_config.cred_secr, ASTARTE_PAIRING_CRED_SECR_LEN + 1);
    if (astarte_err != ASTARTE_OK) {
        return -1;
    }

    LOG_WRN("Credential secret: '%s'", device_config.cred_secr); // NOLINT

    astarte_device_t device;
    astarte_err = astarte_device_init(&device_config, &device);
    if (astarte_err != ASTARTE_OK) {
        return -1;
    }

    while (1) {
        int value = unit_testable_component_get_value(0);
        LOG_INF("Hello world! %s, %d", CONFIG_BOARD, value); // NOLINT
        k_msleep(CONFIG_SLEEP_MS); // sleep for 1 second
    }
    return 0;
}
