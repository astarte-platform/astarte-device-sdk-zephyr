/*
 * (C) Copyright 2024, SECO Mind Srl
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "storage/key_value.h"
#include "storage/key_value_pair.h"

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
static SYS_MUTEX_DEFINE(astarte_kv_storage_mutex);

/************************************************
 *         Static functions declaration         *
 ***********************************************/

/**
 * @brief Find the NVS base ID for the a key-value pair.
 *
 * @param[inout] nvs_fs NVS file system to use.
 * @param[in] stored_pairs Number of total stored key-value pairs.
 * @param[in] namespace Namespace for the key-value pair.
 * @param[in] key Key to use for the search.
 * @param[out] base_id Found base ID for the key-value pair.
 * @return ASTARTE_RESULT_OK if found, ASTARTE_RESULT_NOT_FOUND if not, or error code.
 */
static astarte_result_t find_pair_base_id(struct nvs_fs *nvs_fs, uint16_t stored_pairs,
    const char *namespace, const char *key, uint16_t *base_id);

/************************************************
 *         Global functions definitions         *
 ***********************************************/

astarte_result_t storage_key_value_open(storage_key_value_cfg_t config, struct nvs_fs *nvs_fs)
{
    astarte_result_t ares = ASTARTE_RESULT_OK;
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

    // TODO check if this down casting is legal
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

    ASTARTE_LOG_DBG("Checking for interrupted delete operations...");
    ares = storage_key_value_pair_remove_duplicates(nvs_fs);
    if (ares != ASTARTE_RESULT_OK) {
        ASTARTE_LOG_WRN("Recovery check failed: %s. Storage might contain duplicates.",
            astarte_result_to_name(ares));
    }

    return ASTARTE_RESULT_OK;
}

astarte_result_t storage_key_value_new(
    struct nvs_fs *nvs_fs, const char *namespace, storage_key_value_t *kv_storage)
{
    astarte_result_t ares = ASTARTE_RESULT_OK;
    char *namespace_cpy = NULL;
    size_t namespace_cpy_size = 0U;

    namespace_cpy_size = strlen(namespace) + 1;
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

void storage_key_value_destroy(storage_key_value_t *kv_storage)
{
    free(kv_storage->namespace);
}

astarte_result_t storage_key_value_insert(
    storage_key_value_t *kv_storage, const char *key, const void *value, size_t value_size)
{
    astarte_result_t ares = ASTARTE_RESULT_OK;
    uint16_t stored_pairs = 0;
    uint16_t base_id = 0U;
    bool append_at_end = false;

    int mutex_rc = sys_mutex_lock(&astarte_kv_storage_mutex, K_FOREVER);
    ASTARTE_LOG_COND_ERR(mutex_rc != 0, "System mutex lock failed with %d", mutex_rc);
    __ASSERT_NO_MSG(mutex_rc == 0);

    ares = storage_key_value_pair_get_pairs_number(kv_storage->nvs_fs, &stored_pairs);
    if (ares != ASTARTE_RESULT_OK) {
        ASTARTE_LOG_ERR("Get total stored pairs failed %s.", astarte_result_to_name(ares));
        goto exit;
    }

    // Check if key is already present in storage
    ares
        = find_pair_base_id(kv_storage->nvs_fs, stored_pairs, kv_storage->namespace, key, &base_id);
    if (ares == ASTARTE_RESULT_NOT_FOUND) {
        append_at_end = true;
        // Check capacity and safely allocate the new ID
        ares = storage_key_value_pair_get_new_base_id(stored_pairs, &base_id);
        if (ares != ASTARTE_RESULT_OK) {
            goto exit;
        }
    } else if (ares != ASTARTE_RESULT_OK) {
        ASTARTE_LOG_ERR("Check for old values failed %s.", astarte_result_to_name(ares));
        goto exit;
    }

    ares = storage_key_value_pair_write_pair(
        kv_storage->nvs_fs, base_id, kv_storage->namespace, key, value, value_size);
    if (ares != ASTARTE_RESULT_OK) {
        ASTARTE_LOG_ERR("Insert failed %s.", astarte_result_to_name(ares));
        goto exit;
    }

    if (append_at_end) {
        stored_pairs++;
        ares = storage_key_value_pair_set_pairs_number(kv_storage->nvs_fs, stored_pairs);
        if (ares != ASTARTE_RESULT_OK) {
            ASTARTE_LOG_ERR("Update total stored pairs failed %s.", astarte_result_to_name(ares));
            goto exit;
        }
    }

exit:
    mutex_rc = sys_mutex_unlock(&astarte_kv_storage_mutex);
    ASTARTE_LOG_COND_ERR(mutex_rc != 0, "System mutex unlock failed with %d", mutex_rc);
    __ASSERT_NO_MSG(mutex_rc == 0);

    return ares;
}

astarte_result_t storage_key_value_find(
    storage_key_value_t *kv_storage, const char *key, void *value, size_t *value_size)
{
    astarte_result_t ares = ASTARTE_RESULT_OK;
    uint16_t stored_pairs = 0;
    uint16_t base_id = 0U;

    int mutex_rc = sys_mutex_lock(&astarte_kv_storage_mutex, K_FOREVER);
    ASTARTE_LOG_COND_ERR(mutex_rc != 0, "System mutex lock failed with %d", mutex_rc);
    __ASSERT_NO_MSG(mutex_rc == 0);

    ares = storage_key_value_pair_get_pairs_number(kv_storage->nvs_fs, &stored_pairs);
    if (ares != ASTARTE_RESULT_OK) {
        ASTARTE_LOG_ERR("Get total stored pairs failed %s.", astarte_result_to_name(ares));
        goto exit;
    }

    ares
        = find_pair_base_id(kv_storage->nvs_fs, stored_pairs, kv_storage->namespace, key, &base_id);
    if (ares != ASTARTE_RESULT_OK) {
        goto exit;
    }

    ares = storage_key_value_pair_read_value(kv_storage->nvs_fs, base_id, value, value_size);
    if (ares != ASTARTE_RESULT_OK) {
        ASTARTE_LOG_ERR("Get value of key-value storage failed %s.", astarte_result_to_name(ares));
    }

exit:
    mutex_rc = sys_mutex_unlock(&astarte_kv_storage_mutex);
    ASTARTE_LOG_COND_ERR(mutex_rc != 0, "System mutex unlock failed with %d", mutex_rc);
    __ASSERT_NO_MSG(mutex_rc == 0);

    return ares;
}

astarte_result_t storage_key_value_delete(storage_key_value_t *kv_storage, const char *key)
{
    astarte_result_t ares = ASTARTE_RESULT_OK;
    uint16_t stored_pairs = 0U;
    uint16_t base_id = 0U;

    int mutex_rc = sys_mutex_lock(&astarte_kv_storage_mutex, K_FOREVER);
    ASTARTE_LOG_COND_ERR(mutex_rc != 0, "System mutex lock failed with %d", mutex_rc);
    __ASSERT_NO_MSG(mutex_rc == 0);

    ares = storage_key_value_pair_get_pairs_number(kv_storage->nvs_fs, &stored_pairs);
    if (ares != ASTARTE_RESULT_OK) {
        ASTARTE_LOG_ERR("Get total stored pairs failed %s.", astarte_result_to_name(ares));
        goto exit;
    }

    ares
        = find_pair_base_id(kv_storage->nvs_fs, stored_pairs, kv_storage->namespace, key, &base_id);
    if (ares != ASTARTE_RESULT_OK) {
        goto exit;
    }

    // Determine the ID of the last element in the array
    uint16_t last_base_id = storage_key_value_pair_get_pair_base_id(stored_pairs - 1);

    // If the item to delete is NOT the last one, swap the last one into this slot
    if (base_id != last_base_id) {
        ares = storage_key_value_pair_relocate_pair(kv_storage->nvs_fs, base_id, last_base_id);
        if (ares != ASTARTE_RESULT_OK) {
            ASTARTE_LOG_ERR("Relocation (swap) failed %s.", astarte_result_to_name(ares));
            goto exit;
        }
    }

    stored_pairs--;
    ares = storage_key_value_pair_set_pairs_number(kv_storage->nvs_fs, stored_pairs);
    if (ares != ASTARTE_RESULT_OK) {
        ASTARTE_LOG_ERR("Update total stored pairs failed %s.", astarte_result_to_name(ares));
        goto exit;
    }

exit:
    mutex_rc = sys_mutex_unlock(&astarte_kv_storage_mutex);
    ASTARTE_LOG_COND_ERR(mutex_rc != 0, "System mutex unlock failed with %d", mutex_rc);
    __ASSERT_NO_MSG(mutex_rc == 0);

    return ares;
}

astarte_result_t storage_key_value_iterator_init(
    storage_key_value_t *kv_storage, storage_key_value_iter_t *iter)
{
    astarte_result_t ares = ASTARTE_RESULT_OK;
    uint16_t stored_pairs = 0;
    char *namespace = NULL;
    size_t namespace_size = 0;

    int mutex_rc = sys_mutex_lock(&astarte_kv_storage_mutex, K_FOREVER);
    ASTARTE_LOG_COND_ERR(mutex_rc != 0, "System mutex lock failed with %d", mutex_rc);
    __ASSERT_NO_MSG(mutex_rc == 0);

    ares = storage_key_value_pair_get_pairs_number(kv_storage->nvs_fs, &stored_pairs);
    if (ares != ASTARTE_RESULT_OK) {
        goto exit;
    }

    // Pre allocate space for the namespace
    namespace_size = strlen(kv_storage->namespace) + 1;
    namespace = calloc(namespace_size, sizeof(char));
    if (!namespace) {
        ares = ASTARTE_RESULT_OUT_OF_MEMORY;
        goto exit;
    }

    // Iterate backwards to find the first match
    for (int32_t pair_number = stored_pairs - 1; pair_number >= 0; pair_number--) {
        uint16_t base_id = storage_key_value_pair_get_pair_base_id(pair_number);
        size_t size = 0;

        ares = storage_key_value_pair_read_namespace(kv_storage->nvs_fs, base_id, NULL, &size);
        if (ares != ASTARTE_RESULT_OK) {
            goto exit;
        }

        if (size == namespace_size) {
            ares = storage_key_value_pair_read_namespace(
                kv_storage->nvs_fs, base_id, namespace, &size);
            if (ares != ASTARTE_RESULT_OK) {
                goto exit;
            }

            if (strcmp(kv_storage->namespace, namespace) == 0) {
                iter->kv_storage = kv_storage;
                iter->current_pair = pair_number;
                ares = ASTARTE_RESULT_OK;
                goto exit;
            }
        }
    }

    ares = ASTARTE_RESULT_NOT_FOUND;

exit:
    mutex_rc = sys_mutex_unlock(&astarte_kv_storage_mutex);
    ASTARTE_LOG_COND_ERR(mutex_rc != 0, "System mutex unlock failed with %d", mutex_rc);
    __ASSERT_NO_MSG(mutex_rc == 0);

    free(namespace);
    return ares;
}

astarte_result_t storage_key_value_iterator_next(storage_key_value_iter_t *iter)
{
    astarte_result_t ares = ASTARTE_RESULT_OK;
    char *namespace = NULL;
    size_t namespace_size = 0;

    int mutex_rc = sys_mutex_lock(&astarte_kv_storage_mutex, K_FOREVER);
    ASTARTE_LOG_COND_ERR(mutex_rc != 0, "System mutex lock failed with %d", mutex_rc);
    __ASSERT_NO_MSG(mutex_rc == 0);

    // Pre allocate space for the namespace
    namespace_size = strlen(iter->kv_storage->namespace) + 1;
    namespace = calloc(namespace_size, sizeof(char));
    if (!namespace) {
        ares = ASTARTE_RESULT_OUT_OF_MEMORY;
        goto exit;
    }

    // Go backwards from current_pair -1 looking for pairs in the same namespace
    for (int32_t pair_number = iter->current_pair - 1; pair_number >= 0; pair_number--) {
        uint16_t base_id = storage_key_value_pair_get_pair_base_id(pair_number);
        size_t size = 0;

        ares
            = storage_key_value_pair_read_namespace(iter->kv_storage->nvs_fs, base_id, NULL, &size);
        if (ares != ASTARTE_RESULT_OK) {
            goto exit;
        }

        if (size == namespace_size) {
            ares = storage_key_value_pair_read_namespace(
                iter->kv_storage->nvs_fs, base_id, namespace, &size);
            if (ares != ASTARTE_RESULT_OK) {
                goto exit;
            }

            if (strcmp(iter->kv_storage->namespace, namespace) == 0) {
                iter->current_pair = pair_number;
                ares = ASTARTE_RESULT_OK;
                goto exit;
            }
        }
    }

    ares = ASTARTE_RESULT_NOT_FOUND;

exit:
    mutex_rc = sys_mutex_unlock(&astarte_kv_storage_mutex);
    ASTARTE_LOG_COND_ERR(mutex_rc != 0, "System mutex unlock failed with %d", mutex_rc);
    __ASSERT_NO_MSG(mutex_rc == 0);

    free(namespace);
    return ares;
}

astarte_result_t storage_key_value_iterator_get(
    storage_key_value_iter_t *iter, void *key, size_t *key_size)
{
    astarte_result_t ares = ASTARTE_RESULT_OK;

    int mutex_rc = sys_mutex_lock(&astarte_kv_storage_mutex, K_FOREVER);
    ASTARTE_LOG_COND_ERR(mutex_rc != 0, "System mutex lock failed with %d", mutex_rc);
    __ASSERT_NO_MSG(mutex_rc == 0);

    uint16_t base_id = storage_key_value_pair_get_pair_base_id(iter->current_pair);
    ares = storage_key_value_pair_read_key(iter->kv_storage->nvs_fs, base_id, key, key_size);

    mutex_rc = sys_mutex_unlock(&astarte_kv_storage_mutex);
    ASTARTE_LOG_COND_ERR(mutex_rc != 0, "System mutex unlock failed with %d", mutex_rc);
    __ASSERT_NO_MSG(mutex_rc == 0);

    return ares;
}

/************************************************
 *         Static functions definitions         *
 ***********************************************/

// This function is still understandable
// NOLINTNEXTLINE(readability-function-cognitive-complexity)
static astarte_result_t find_pair_base_id(struct nvs_fs *nvs_fs, uint16_t stored_pairs,
    const char *namespace, const char *key, uint16_t *base_id)
{
    astarte_result_t ares = ASTARTE_RESULT_OK;
    char *tmp_namespace = NULL;
    char *tmp_key = NULL;
    bool found = false;
    uint16_t pair_number = 0U;

    size_t nsp_len = strlen(namespace) + 1;
    size_t key_len = strlen(key) + 1;

    // Pre allocate space for the key and namespace
    tmp_namespace = calloc(nsp_len, sizeof(char));
    tmp_key = calloc(key_len, sizeof(char));
    if (!tmp_namespace || !tmp_key) {
        ares = ASTARTE_RESULT_OUT_OF_MEMORY;
        goto exit;
    }

    for (; pair_number < stored_pairs; pair_number++) {
        uint16_t curr_base_id = storage_key_value_pair_get_pair_base_id(pair_number);
        size_t size = 0;

        ares = storage_key_value_pair_read_namespace(nvs_fs, curr_base_id, NULL, &size);
        if (ares != ASTARTE_RESULT_OK) {
            goto exit;
        }

        // If size doesn't match, skip to the next pair
        if (size != nsp_len) {
            continue;
        }

        ares = storage_key_value_pair_read_namespace(nvs_fs, curr_base_id, tmp_namespace, &size);
        if (ares != ASTARTE_RESULT_OK) {
            goto exit;
        }

        // If namespace string doesn't match, skip to the next pair
        if (strcmp(tmp_namespace, namespace) != 0) {
            continue;
        }

        ares = storage_key_value_pair_read_key(nvs_fs, curr_base_id, NULL, &size);
        if (ares != ASTARTE_RESULT_OK) {
            goto exit;
        }

        // If key size doesn't match, skip to the next pair
        if (size != key_len) {
            continue;
        }

        ares = storage_key_value_pair_read_key(nvs_fs, curr_base_id, tmp_key, &size);
        if (ares != ASTARTE_RESULT_OK) {
            goto exit;
        }

        // If the key matches, we found our pair
        if (strcmp(tmp_key, key) == 0) {
            found = true;
            break;
        }
    }

    if (!found) {
        ares = ASTARTE_RESULT_NOT_FOUND;
        goto exit;
    }

    *base_id = storage_key_value_pair_get_pair_base_id(pair_number);

exit:
    free(tmp_namespace);
    free(tmp_key);

    return ares;
}
