/*
 * (C) Copyright 2026, SECO Mind Srl
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "storage/core.h"

#include <string.h>

#include <zephyr/drivers/flash.h>
#include <zephyr/storage/flash_map.h>
#include <zephyr/version.h>

#if (KERNEL_VERSION_MAJOR >= 4) && (KERNEL_VERSION_MINOR >= 4)
#include <zephyr/kvss/nvs.h>
#else
#include <zephyr/fs/nvs.h>
#endif

#include "log.h"
ASTARTE_LOG_MODULE_REGISTER(device_storage, CONFIG_ASTARTE_DEVICE_SDK_DEVICE_STORAGE_LOG_LEVEL);

/************************************************
 *        Defines, constants and typedef        *
 ***********************************************/

#define NVS_PARTITION astarte_partition
#if !FIXED_PARTITION_EXISTS(NVS_PARTITION)
#error "Permanent storage is enabled but 'astarte_partition' flash partition is missing."
#endif // FIXED_PARTITION_EXISTS(NVS_PARTITION)

#if (KERNEL_VERSION_MAJOR >= 4) && (KERNEL_VERSION_MINOR >= 4)
#define NVS_PARTITION_DEVICE PARTITION_DEVICE(NVS_PARTITION)
#define NVS_PARTITION_OFFSET PARTITION_OFFSET(NVS_PARTITION)
#define NVS_PARTITION_SIZE PARTITION_SIZE(NVS_PARTITION)
#else
#define NVS_PARTITION_DEVICE FIXED_PARTITION_DEVICE(NVS_PARTITION)
#define NVS_PARTITION_OFFSET FIXED_PARTITION_OFFSET(NVS_PARTITION)
#define NVS_PARTITION_SIZE FIXED_PARTITION_SIZE(NVS_PARTITION)
#endif

#define SYNCHRONIZATION_NAMESPACE "synchronization_namespace"
#define INTROSPECTION_NAMESPACE "introspection_namespace"
#define PROPERTIES_NAMESPACE "properties_namespace"

/************************************************
 *         Global functions definitions         *
 ***********************************************/

astarte_result_t storage_init(storage_data_t *handle)
{
    astarte_result_t ares = ASTARTE_RESULT_OK;

    if (!handle) {
        return ASTARTE_RESULT_INVALID_PARAM;
    }

    // Zero out the memory to ensure clean state
    memset(handle, 0, sizeof(storage_data_t));

    // Open the key value storage flash partition
    storage_key_value_cfg_t kv_storage_cfg = {
        .flash_device = NVS_PARTITION_DEVICE,
        .flash_offset = NVS_PARTITION_OFFSET,
        .flash_partition_size = NVS_PARTITION_SIZE,
    };
    ares = storage_key_value_open(kv_storage_cfg, &handle->nvs_fs);
    if (ares != ASTARTE_RESULT_OK) {
        ASTARTE_LOG_ERR("Error opening cache: %s.", astarte_result_to_name(ares));
        return ares;
    }

    // Init Synchronization Storage
    ares = storage_key_value_new(&handle->nvs_fs, SYNCHRONIZATION_NAMESPACE, &handle->sync_storage);
    if (ares != ASTARTE_RESULT_OK) {
        return ares;
    }

    // Init Introspection Storage
    ares = storage_key_value_new(&handle->nvs_fs, INTROSPECTION_NAMESPACE, &handle->intro_storage);
    if (ares != ASTARTE_RESULT_OK) {
        storage_key_value_destroy(&handle->sync_storage); // Rollback
        return ares;
    }

    // Init Properties Storage
    ares = storage_key_value_new(&handle->nvs_fs, PROPERTIES_NAMESPACE, &handle->prop_storage);
    if (ares != ASTARTE_RESULT_OK) {
        storage_key_value_destroy(&handle->sync_storage);
        storage_key_value_destroy(&handle->intro_storage);
        return ares;
    }

    handle->initialized = true;
    return ASTARTE_RESULT_OK;
}

void storage_destroy(storage_data_t *handle)
{
    if (!handle || !handle->initialized) {
        return;
    }

    // Destroy individual storage instances
    storage_key_value_destroy(&handle->sync_storage);
    storage_key_value_destroy(&handle->intro_storage);
    storage_key_value_destroy(&handle->prop_storage);

    handle->initialized = false;
}
