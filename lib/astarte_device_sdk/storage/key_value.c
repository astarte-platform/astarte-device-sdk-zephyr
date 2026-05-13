/*
 * (C) Copyright 2024, SECO Mind Srl
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "storage/key_value.h"
#include "storage/key_value_entry.h"

#include <stdlib.h>
#include <string.h>

#include <zephyr/drivers/flash.h>
#include <zephyr/kernel.h>
#include <zephyr/storage/flash_map.h>
#include <zephyr/sys/mutex.h>
#include <zephyr/version.h>

#if (KERNEL_VERSION_MAJOR >= 4) && (KERNEL_VERSION_MINOR >= 4)
#include <zephyr/kvss/nvs.h>
#else
#include <zephyr/fs/nvs.h>
#endif

#include "log.h"

ASTARTE_LOG_MODULE_REGISTER(astarte_kv_storage, CONFIG_ASTARTE_DEVICE_SDK_KV_STORAGE_LOG_LEVEL);

/************************************************
 *        Defines, constants and typedef        *
 ***********************************************/

// This mutex will be shared by all instances of this driver.
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
static SYS_MUTEX_DEFINE(astarte_storage_key_value_mutex);

/************************************************
 *         Global functions definitions         *
 ***********************************************/

astarte_result_t astarte_storage_key_value_open(
    astarte_storage_key_value_cfg_t config, struct nvs_fs *nvs_fs)
{
    struct flash_pages_info fp_info = { 0 };

    if (!device_is_ready(config.flash_device)) {
        ASTARTE_LOG_ERR("Flash device %s not ready.", config.flash_device->name);
        return ASTARTE_RESULT_DEVICE_NOT_READY;
    }

    int flash_rc = flash_get_page_info_by_offs(config.flash_device, config.flash_offset, &fp_info);
    if (flash_rc) {
        ASTARTE_LOG_ERR("Unable to get page info: %d.", flash_rc);
        return ASTARTE_RESULT_INTERNAL_ERROR;
    }

    uint16_t flash_sector_count = (uint16_t) (config.flash_partition_size / fp_info.size);

    memset(nvs_fs, 0, sizeof(struct nvs_fs));
    nvs_fs->flash_device = config.flash_device;
    nvs_fs->offset = config.flash_offset;
    nvs_fs->sector_size = fp_info.size;
    nvs_fs->sector_count = flash_sector_count;

    int nvs_rc = nvs_mount(nvs_fs);
    if (nvs_rc) {
        ASTARTE_LOG_ERR("NVS mount error: %s (%d).", strerror(-nvs_rc), nvs_rc);
        return ASTARTE_RESULT_NVS_ERROR;
    }

    return ASTARTE_RESULT_OK;
}

astarte_result_t astarte_storage_key_value_new(
    struct nvs_fs *nvs_fs, const char *namespace, astarte_storage_key_value_t *kv_storage)
{
    astarte_result_t ares = ASTARTE_RESULT_OK;
    char *namespace_cpy = NULL;
    size_t namespace_cpy_size = strlen(namespace) + 1;

    namespace_cpy = calloc(namespace_cpy_size, sizeof(char));
    if (!namespace_cpy) {
        ASTARTE_LOG_ERR("Out of memory %s: %d", __FILE__, __LINE__);
        ares = ASTARTE_RESULT_OUT_OF_MEMORY;
        goto error;
    }
    strncpy(namespace_cpy, namespace, namespace_cpy_size);

    kv_storage->namespace = namespace_cpy;
    kv_storage->nvs_fs = nvs_fs;

    return ASTARTE_RESULT_OK;

error:
    free(namespace_cpy);
    kv_storage->namespace = NULL;
    return ares;
}

void astarte_storage_key_value_destroy(astarte_storage_key_value_t *kv_storage)
{
    free(kv_storage->namespace);
    kv_storage->namespace = NULL;
}

astarte_result_t astarte_storage_key_value_insert(
    astarte_storage_key_value_t *kv_storage, const char *key, const void *value, size_t value_size)
{
    astarte_result_t ares = ASTARTE_RESULT_OK;
    uint16_t entry_id = 0;

    int mutex_rc = sys_mutex_lock(&astarte_storage_key_value_mutex, K_FOREVER);
    ASTARTE_LOG_COND_ERR(mutex_rc != 0, "System mutex lock failed with %d", mutex_rc);
    __ASSERT_NO_MSG(mutex_rc == 0);

    ares = astarte_storage_key_value_entry_find_or_alloc(
        kv_storage->nvs_fs, kv_storage->namespace, key, &entry_id, true);
    if (ares != ASTARTE_RESULT_OK) {
        ASTARTE_LOG_ERR("Key finding/allocation failed %s.", astarte_result_to_name(ares));
        goto exit;
    }

    ares = astarte_storage_key_value_entry_write(
        kv_storage->nvs_fs, entry_id, kv_storage->namespace, key, value, value_size);
    if (ares != ASTARTE_RESULT_OK) {
        ASTARTE_LOG_ERR("Insert failed %s.", astarte_result_to_name(ares));
    }

exit:
    mutex_rc = sys_mutex_unlock(&astarte_storage_key_value_mutex);
    ASTARTE_LOG_COND_ERR(mutex_rc != 0, "System mutex unlock failed with %d", mutex_rc);
    __ASSERT_NO_MSG(mutex_rc == 0);

    return ares;
}

astarte_result_t astarte_storage_key_value_find(
    astarte_storage_key_value_t *kv_storage, const char *key, void *value, size_t *value_size)
{
    astarte_result_t ares = ASTARTE_RESULT_OK;
    uint16_t entry_id = 0;

    int mutex_rc = sys_mutex_lock(&astarte_storage_key_value_mutex, K_FOREVER);
    ASTARTE_LOG_COND_ERR(mutex_rc != 0, "System mutex lock failed with %d", mutex_rc);
    __ASSERT_NO_MSG(mutex_rc == 0);

    ares = astarte_storage_key_value_entry_find_or_alloc(
        kv_storage->nvs_fs, kv_storage->namespace, key, &entry_id, false);
    if (ares != ASTARTE_RESULT_OK) {
        // No error logs as this could be a not found case, which is not necessarily an error
        goto exit;
    }

    ares = astarte_storage_key_value_entry_read_value(
        kv_storage->nvs_fs, entry_id, value, value_size);
    if (ares != ASTARTE_RESULT_OK) {
        ASTARTE_LOG_ERR("Get value of key-value storage failed %s.", astarte_result_to_name(ares));
    }

exit:
    mutex_rc = sys_mutex_unlock(&astarte_storage_key_value_mutex);
    ASTARTE_LOG_COND_ERR(mutex_rc != 0, "System mutex unlock failed with %d", mutex_rc);
    __ASSERT_NO_MSG(mutex_rc == 0);

    return ares;
}

astarte_result_t astarte_storage_key_value_delete(
    astarte_storage_key_value_t *kv_storage, const char *key)
{
    astarte_result_t ares = ASTARTE_RESULT_OK;
    uint16_t entry_id = 0;

    int mutex_rc = sys_mutex_lock(&astarte_storage_key_value_mutex, K_FOREVER);
    ASTARTE_LOG_COND_ERR(mutex_rc != 0, "System mutex lock failed with %d", mutex_rc);
    __ASSERT_NO_MSG(mutex_rc == 0);

    ares = astarte_storage_key_value_entry_find_or_alloc(
        kv_storage->nvs_fs, kv_storage->namespace, key, &entry_id, false);
    if (ares != ASTARTE_RESULT_OK) {
        // No error logs as this could be a not found case, which is not necessarily an error
        goto exit;
    }

    ssize_t ret = nvs_delete(kv_storage->nvs_fs, entry_id);
    if (ret < 0 && ret != -ENOENT) {
        ares = ASTARTE_RESULT_NVS_ERROR;
        ASTARTE_LOG_ERR("NVS Delete Error: %d.", (int) ret);
    }

exit:
    mutex_rc = sys_mutex_unlock(&astarte_storage_key_value_mutex);
    ASTARTE_LOG_COND_ERR(mutex_rc != 0, "System mutex unlock failed with %d", mutex_rc);
    __ASSERT_NO_MSG(mutex_rc == 0);

    return ares;
}

astarte_result_t astarte_storage_key_value_iterator_init(
    astarte_storage_key_value_t *kv_storage, astarte_storage_key_value_iter_t *iter)
{
    // ID 0 is a reserved starting point for the iterator
    iter->kv_storage = kv_storage;
    iter->current_id = 0;
    return astarte_storage_key_value_iterator_next(iter);
}

astarte_result_t astarte_storage_key_value_iterator_next(astarte_storage_key_value_iter_t *iter)
{
    astarte_result_t ares = ASTARTE_RESULT_NOT_FOUND;
    bool matches = false;

    int mutex_rc = sys_mutex_lock(&astarte_storage_key_value_mutex, K_FOREVER);
    ASTARTE_LOG_COND_ERR(mutex_rc != 0, "System mutex lock failed with %d", mutex_rc);
    __ASSERT_NO_MSG(mutex_rc == 0);

    for (uint32_t idx = iter->current_id + 1; idx < UINT16_MAX; idx++) {
        ares = astarte_storage_key_value_entry_check_namespace(
            iter->kv_storage->nvs_fs, (uint16_t) idx, iter->kv_storage->namespace, &matches);

        if (ares == ASTARTE_RESULT_OK && matches) {
            iter->current_id = (uint16_t) idx;
            goto exit;
        }
    }

    ASTARTE_LOG_DBG("Iterator reached the end.");
    ares = ASTARTE_RESULT_NOT_FOUND;

exit:
    mutex_rc = sys_mutex_unlock(&astarte_storage_key_value_mutex);
    ASTARTE_LOG_COND_ERR(mutex_rc != 0, "System mutex unlock failed with %d", mutex_rc);
    __ASSERT_NO_MSG(mutex_rc == 0);

    return ares;
}

astarte_result_t astarte_storage_key_value_iterator_get(
    astarte_storage_key_value_iter_t *iter, void *key, size_t *key_size)
{
    astarte_result_t ares = ASTARTE_RESULT_OK;

    int mutex_rc = sys_mutex_lock(&astarte_storage_key_value_mutex, K_FOREVER);
    ASTARTE_LOG_COND_ERR(mutex_rc != 0, "System mutex lock failed with %d", mutex_rc);
    __ASSERT_NO_MSG(mutex_rc == 0);

    ares = astarte_storage_key_value_entry_read_key(
        iter->kv_storage->nvs_fs, iter->current_id, (char *) key, key_size);

    mutex_rc = sys_mutex_unlock(&astarte_storage_key_value_mutex);
    ASTARTE_LOG_COND_ERR(mutex_rc != 0, "System mutex unlock failed with %d", mutex_rc);
    __ASSERT_NO_MSG(mutex_rc == 0);

    return ares;
}
