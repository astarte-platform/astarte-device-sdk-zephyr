/*
 * (C) Copyright 2024, SECO Mind Srl
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdlib.h>

#include <zephyr/data/json.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/atomic.h>
#include <zephyr/sys/util.h>
#include <zephyr/sys_clock.h>

#if defined(CONFIG_ARCH_POSIX)
#include <nsi_main.h>
#endif

#if (!defined(CONFIG_ASTARTE_DEVICE_SDK_DEVELOP_USE_NON_TLS_HTTP)                                  \
    || !defined(CONFIG_ASTARTE_DEVICE_SDK_DEVELOP_USE_NON_TLS_MQTT))
/**
 * \def MBEDTLS_DEBUG_C
 *
 * Enable mbed tls debug
 */
#define MBEDTLS_DEBUG_C

#if defined(CMAKE_CUSTOM_GENERATED_CERTIFICATE)
#include "ca_certificate_inc.h"
#else
#error TLS is enabled but no generated certificate was found: check the CERTIFICATE file content
#endif

#include <mbedtls/debug.h>
#include <zephyr/net/tls_credentials.h>
#endif

#include "eth.h"

#include "e2erunner.h"
#include "e2eutilities.h"

LOG_MODULE_REGISTER(main, CONFIG_APP_LOG_LEVEL); // NOLINT

/************************************************
 *       Checks over configuration values       *
 ***********************************************/

BUILD_ASSERT(CONFIG_ARCH_POSIX == 1, "The e2e test needs to run on the native_sim board");
BUILD_ASSERT(
    sizeof(CONFIG_DEVICE_ID) == ASTARTE_PAIRING_DEVICE_ID_LEN + 1, "Missing device ID in e2e tets");
BUILD_ASSERT(sizeof(CONFIG_CREDENTIAL_SECRET) == ASTARTE_PAIRING_CRED_SECR_LEN + 1,
    "Missing credential secret in e2e test");

/************************************************
 * Constants, static variables and defines
 ***********************************************/

K_THREAD_STACK_DEFINE(eth_thread_stack_area, 4096);
static struct k_thread eth_thread_data;

enum e2e_thread_flags
{
    THREAD_TERMINATION_FLAG = 0,
};
static atomic_t device_thread_flags;

/************************************************
 * Static functions declaration
 ***********************************************/

static void eth_thread_entry_point(void *unused1, void *unused2, void *unused3);

/************************************************
 * Global functions definition
 ***********************************************/

int main(void)
{
    LOG_INF("Astarte device e2e test"); // NOLINT

    // Initialize Ethernet driver
    LOG_INF("Initializing Ethernet driver."); // NOLINT
    if (eth_connect() != 0) {
        LOG_ERR("Connectivity intialization failed!"); // NOLINT
        return -1;
    }

#if !(defined(CONFIG_ASTARTE_DEVICE_SDK_DEVELOP_USE_NON_TLS_HTTP)                                  \
    || defined(CONFIG_ASTARTE_DEVICE_SDK_DEVELOP_USE_NON_TLS_MQTT))
    // Add TLS certificate
    tls_credential_add(CONFIG_ASTARTE_DEVICE_SDK_HTTPS_CA_CERT_TAG, TLS_CREDENTIAL_CA_CERTIFICATE,
        ca_certificate_root, ARRAY_SIZE(ca_certificate_root));

    LOG_INF("The certificate is:\n%s", ca_certificate_root); // NOLINT

    // enable mbedtls logging
    mbedtls_debug_set_threshold(1);
#endif

    LOG_INF("Spawning a new thread to poll the eth interface and check connectivity."); // NOLINT
    k_thread_create(&eth_thread_data, eth_thread_stack_area,
        K_THREAD_STACK_SIZEOF(eth_thread_stack_area), eth_thread_entry_point, NULL, NULL, NULL,
        CONFIG_DEVICE_THREAD_PRIORITY, 0, K_NO_WAIT);

    LOG_INF("Running e2e test."); // NOLINT
    run_e2e_test();

    atomic_set_bit(&device_thread_flags, THREAD_TERMINATION_FLAG);
    CHECK_HALT(k_thread_join(&eth_thread_data, K_FOREVER) != 0,
        "Failed in waiting for the eth polling thread to terminate.");

    LOG_INF("Returning from the e2e test."); // NOLINT

    // we know we are running on POSIX because it is checked at build time (view BUILD_ASSERT)
    nsi_exit(0);
    return 1;
}

static void eth_thread_entry_point(void *unused1, void *unused2, void *unused3)
{
    (void) unused1;
    (void) unused2;
    (void) unused3;

    LOG_INF("Starting eth polling thread"); // NOLINT

    while (!atomic_test_bit(&device_thread_flags, THREAD_TERMINATION_FLAG)) {
        k_timepoint_t timepoint = sys_timepoint_calc(K_MSEC(CONFIG_ETH_POLL_PERIOD_MS));

        eth_poll();

        k_sleep(sys_timepoint_timeout(timepoint));
    }
}