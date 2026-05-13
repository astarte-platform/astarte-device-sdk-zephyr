/*
 * (C) Copyright 2024, SECO Mind Srl
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "storage/key_value_entry.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <zephyr/sys/crc.h>

#include "log.h"

ASTARTE_LOG_MODULE_DECLARE(astarte_kv_storage, CONFIG_ASTARTE_DEVICE_SDK_KV_STORAGE_LOG_LEVEL);

/************************************************
 *        Defines, constants and typedef        *
 ***********************************************/

#define ENTRY_HEADER_NAMESPACE_LEN_BYTES 2
#define ENTRY_HEADER_KEY_LEN_BYTES 2
#define ENTRY_HEADER_LENGTHS_BYTES (ENTRY_HEADER_NAMESPACE_LEN_BYTES + ENTRY_HEADER_KEY_LEN_BYTES)

/************************************************
 *         Static functions declaration         *
 ***********************************************/

/**
 * @brief Deterministically generates a starting ID for the linear probing hash map.
 *
 * @param[in] namespace Target namespace string.
 * @param[in] key Target key string.
 * @return 16-bit hash value to be used as starting ID for probing.
 */
static uint16_t generate_hash(const char *namespace, const char *key);
/**
 * @brief Helper function to check if a specific NVS entry matches the target namespace and key.
 */
static astarte_result_t check_entry_match(
    struct nvs_fs *nvs_fs, uint16_t curr_id, const char *namespace, const char *key);

/************************************************
 *         Global functions definitions         *
 ***********************************************/

astarte_result_t astarte_storage_key_value_entry_find_or_alloc(
    struct nvs_fs *nvs_fs, const char *namespace, const char *key, uint16_t *idx, bool allocate)
{
    uint16_t start_id = generate_hash(namespace, key);
    uint16_t curr_id = start_id;

    size_t nsp_len = strlen(namespace);
    size_t key_len = strlen(key);

    do {
        // Read the first 4 bytes to get the namespace and key lengths
        uint16_t lengths[ENTRY_HEADER_LENGTHS_BYTES / sizeof(uint16_t)] = { 0 };
        ssize_t ret = nvs_read(nvs_fs, curr_id, lengths, ENTRY_HEADER_LENGTHS_BYTES);

        // Slot is empty
        if (ret == -ENOENT) {
            if (!allocate) {
                ASTARTE_LOG_DBG("Key not found at ID %d", curr_id);
                return ASTARTE_RESULT_NOT_FOUND;
            }
            ASTARTE_LOG_DBG("Found empty slot at ID %d", curr_id);
            *idx = curr_id;
            return ASTARTE_RESULT_OK;
        }

        // Error reading slot
        if (ret < 0) {
            ASTARTE_LOG_ERR("Error reading slot at ID %d, error: %d", curr_id, (int) ret);
            return ASTARTE_RESULT_NVS_ERROR;
        }

        // Check if lengths match expected values before reading the header
        if (ret >= ENTRY_HEADER_LENGTHS_BYTES && lengths[0] == nsp_len && lengths[1] == key_len) {
            // Check if the entry namespace and key match the expected values
            astarte_result_t match_res = check_entry_match(nvs_fs, curr_id, namespace, key);
            if (match_res == ASTARTE_RESULT_OK) {
                *idx = curr_id;
                ASTARTE_LOG_DBG("Found matching entry at ID %d", curr_id);
                return ASTARTE_RESULT_OK;
            }
            if (match_res != ASTARTE_RESULT_NOT_FOUND) {
                return match_res;
            }
            // If ASTARTE_RESULT_NOT_FOUND is returned, it means a collision occurred.
            // The loop will continue to the next ID.
        }

        // Increase the ID to handle collisions
        curr_id++;
        if (curr_id == UINT16_MAX) {
            curr_id = 1;
        }
    } while (curr_id != start_id);

    ASTARTE_LOG_ERR("Key-value storage is full");
    return ASTARTE_RESULT_KV_STORAGE_FULL;
}

astarte_result_t astarte_storage_key_value_entry_write(struct nvs_fs *nvs_fs, uint16_t idx,
    const char *namespace, const char *key, const void *value, size_t value_size)
{
    astarte_result_t ares = ASTARTE_RESULT_OK;
    uint8_t *payload = NULL;
    uint16_t nsp_len = strlen(namespace);
    uint16_t key_len = strlen(key);
    size_t payload_size = ENTRY_HEADER_LENGTHS_BYTES + nsp_len + key_len + value_size;

    payload = k_calloc(payload_size, sizeof(uint8_t));
    if (!payload) {
        ASTARTE_LOG_ERR("Out of memory %s: %d", __FILE__, __LINE__);
        ares = ASTARTE_RESULT_OUT_OF_MEMORY;
        goto exit;
    }

    memcpy(payload, &nsp_len, ENTRY_HEADER_NAMESPACE_LEN_BYTES);
    memcpy(payload + ENTRY_HEADER_NAMESPACE_LEN_BYTES, &key_len, ENTRY_HEADER_KEY_LEN_BYTES);
    // NOLINTNEXTLINE(bugprone-not-null-terminated-result)
    memcpy(payload + ENTRY_HEADER_LENGTHS_BYTES, namespace, nsp_len);
    // NOLINTNEXTLINE(bugprone-not-null-terminated-result)
    memcpy(payload + ENTRY_HEADER_LENGTHS_BYTES + nsp_len, key, key_len);

    if (value_size > 0 && value != NULL) {
        memcpy(payload + ENTRY_HEADER_LENGTHS_BYTES + nsp_len + key_len, value, value_size);
    }

    // Atomic NVS write mapping guarantees safety on sudden power cycles
    ssize_t ret = nvs_write(nvs_fs, idx, payload, payload_size);
    if (ret < 0) {
        ASTARTE_LOG_ERR("Error writing to NVS at ID %d, error: %d", idx, ret);
        ares = ASTARTE_RESULT_NVS_ERROR;
        goto exit;
    }

exit:
    k_free(payload);
    return ares;
}

astarte_result_t astarte_storage_key_value_entry_read_value(
    struct nvs_fs *nvs_fs, uint16_t idx, void *value, size_t *value_size)
{
    astarte_result_t ares = ASTARTE_RESULT_OK;
    uint8_t *buf = NULL;

    // A 'NULL' read returns the full payload size
    ssize_t total_size = nvs_read(nvs_fs, idx, NULL, 0);
    if (total_size < 0) {
        ASTARTE_LOG_ERR("Error reading from NVS at ID %d, error: %d", idx, total_size);
        ares = ASTARTE_RESULT_NVS_ERROR;
        goto exit;
    }

    uint16_t lengths[ENTRY_HEADER_LENGTHS_BYTES / sizeof(uint16_t)] = { 0 };
    ssize_t ret = nvs_read(nvs_fs, idx, lengths, sizeof(lengths));
    if (ret < 0) {
        ASTARTE_LOG_ERR("Error reading header from NVS at ID %d, error: %d", idx, ret);
        ares = ASTARTE_RESULT_NVS_ERROR;
        goto exit;
    }

    size_t header_size = ENTRY_HEADER_LENGTHS_BYTES + lengths[0] + lengths[1];
    if ((size_t) total_size < header_size) {
        ASTARTE_LOG_ERR("Error: Incomplete header at ID %d", idx);
        ares = ASTARTE_RESULT_INTERNAL_ERROR;
        goto exit;
    }

    size_t val_size = total_size - header_size;

    if (!value) {
        *value_size = val_size;
        goto exit;
    }

    if (*value_size < val_size) {
        ASTARTE_LOG_ERR("Error: Value buffer too small at ID %d", idx);
        ares = ASTARTE_RESULT_INVALID_PARAM;
        goto exit;
    }

    buf = k_calloc(total_size, sizeof(uint8_t));
    if (!buf) {
        ASTARTE_LOG_ERR("Out of memory %s: %d", __FILE__, __LINE__);
        ares = ASTARTE_RESULT_OUT_OF_MEMORY;
        goto exit;
    }

    ret = nvs_read(nvs_fs, idx, buf, total_size);
    if (ret < 0) {
        ASTARTE_LOG_ERR("Error reading full payload from NVS at ID %d, error: %d", idx, ret);
        ares = ASTARTE_RESULT_NVS_ERROR;
        goto exit;
    }

    if (val_size > 0) {
        memcpy(value, buf + header_size, val_size);
    }

    *value_size = val_size;

exit:
    k_free(buf);
    return ares;
}

astarte_result_t astarte_storage_key_value_entry_read_key(
    struct nvs_fs *nvs_fs, uint16_t idx, char *key, size_t *key_size)
{
    astarte_result_t ares = ASTARTE_RESULT_OK;
    uint8_t *buf = NULL;

    uint16_t lengths[ENTRY_HEADER_LENGTHS_BYTES / sizeof(uint16_t)] = { 0 };
    ssize_t ret = nvs_read(nvs_fs, idx, lengths, ENTRY_HEADER_LENGTHS_BYTES);
    if (ret < 0) {
        ASTARTE_LOG_ERR("Error reading header from NVS at ID %d, error: %d", idx, ret);
        ares = ASTARTE_RESULT_NVS_ERROR;
        goto exit;
    }

    size_t nsp_len = lengths[0];
    size_t key_len = lengths[1];

    if (!key) {
        *key_size = key_len + 1; // +1 includes room for termination
        goto exit;
    }

    if (*key_size < key_len + 1) {
        ASTARTE_LOG_ERR("Error: Key buffer too small at ID %d", idx);
        ares = ASTARTE_RESULT_INVALID_PARAM;
        goto exit;
    }

    size_t header_size = ENTRY_HEADER_LENGTHS_BYTES + nsp_len + key_len;
    buf = k_calloc(header_size, sizeof(uint8_t));
    if (!buf) {
        ASTARTE_LOG_ERR("Out of memory %s: %d", __FILE__, __LINE__);
        ares = ASTARTE_RESULT_OUT_OF_MEMORY;
        goto exit;
    }

    ret = nvs_read(nvs_fs, idx, buf, header_size);
    if (ret < 0) {
        ASTARTE_LOG_ERR("Error reading header from NVS at ID %d, error: %d", idx, ret);
        ares = ASTARTE_RESULT_NVS_ERROR;
        goto exit;
    }

    memcpy(key, buf + ENTRY_HEADER_LENGTHS_BYTES + nsp_len, key_len);
    key[key_len] = '\0';
    *key_size = key_len + 1;

exit:
    k_free(buf);
    return ares;
}

astarte_result_t astarte_storage_key_value_entry_check_namespace(
    struct nvs_fs *nvs_fs, uint16_t idx, const char *namespace, bool *matches)
{
    astarte_result_t ares = ASTARTE_RESULT_OK;
    uint8_t *buf = NULL;

    uint16_t lengths[ENTRY_HEADER_LENGTHS_BYTES / sizeof(uint16_t)] = { 0 };
    ssize_t ret = nvs_read(nvs_fs, idx, lengths, ENTRY_HEADER_LENGTHS_BYTES);

    if (ret == -ENOENT) {
        *matches = false;
        goto exit;
    } else if (ret < 0) {
        ASTARTE_LOG_ERR("Error reading header from NVS at ID %d, error: %d", idx, ret);
        ares = ASTARTE_RESULT_NVS_ERROR;
        goto exit;
    }

    size_t nsp_len = lengths[0];
    if (nsp_len != strlen(namespace)) {
        *matches = false;
        goto exit;
    }

    size_t header_size = ENTRY_HEADER_LENGTHS_BYTES + nsp_len;
    buf = k_calloc(header_size, sizeof(uint8_t));
    if (!buf) {
        ASTARTE_LOG_ERR("Out of memory %s: %d", __FILE__, __LINE__);
        ares = ASTARTE_RESULT_OUT_OF_MEMORY;
        goto exit;
    }

    ret = nvs_read(nvs_fs, idx, buf, header_size);
    if (ret < 0) {
        ASTARTE_LOG_ERR("Error reading header from NVS at ID %d, error: %d", idx, ret);
        ares = ASTARTE_RESULT_NVS_ERROR;
        goto exit;
    }

    if (strncmp((char *) buf + ENTRY_HEADER_LENGTHS_BYTES, namespace, nsp_len) == 0) {
        *matches = true;
    } else {
        *matches = false;
    }

exit:
    k_free(buf);
    return ares;
}

/************************************************
 *         Static functions definitions         *
 ***********************************************/

static uint16_t generate_hash(const char *namespace, const char *key)
{
    uint16_t crc = UINT16_MAX; // Zephyr's crc16_ccitt seed
    crc = crc16_ccitt(crc, (const uint8_t *) namespace, strlen(namespace));
    crc = crc16_ccitt(crc, (const uint8_t *) key, strlen(key));

    // Fallbacks to avoid invalid Zephyr NVS IDs
    if (crc == UINT16_MAX || crc == 0) {
        crc = 1;
    }
    return crc;
}

static astarte_result_t check_entry_match(
    struct nvs_fs *nvs_fs, uint16_t curr_id, const char *namespace, const char *key)
{
    astarte_result_t ares = ASTARTE_RESULT_NOT_FOUND;
    uint8_t *header = NULL;

    size_t nsp_len = strlen(namespace);
    size_t key_len = strlen(key);
    size_t header_size = ENTRY_HEADER_LENGTHS_BYTES + nsp_len + key_len;

    header = k_calloc(header_size, sizeof(uint8_t));
    if (!header) {
        ASTARTE_LOG_ERR("Out of memory %s: %d", __FILE__, __LINE__);
        ares = ASTARTE_RESULT_OUT_OF_MEMORY;
        goto exit;
    }

    ssize_t ret = nvs_read(nvs_fs, curr_id, header, header_size);
    if (ret < 0) {
        ares = ASTARTE_RESULT_NVS_ERROR;
        goto exit;
    }

    if (ret >= (ssize_t) header_size) {
        int nsp_cmp = strncmp((char *) header + ENTRY_HEADER_LENGTHS_BYTES, namespace, nsp_len);
        int key_cmp = strncmp((char *) header + ENTRY_HEADER_LENGTHS_BYTES + nsp_len, key, key_len);
        if (nsp_cmp == 0 && key_cmp == 0) {
            ares = ASTARTE_RESULT_OK;
            goto exit;
        }
    }

exit:
    k_free(header);
    return ares;
}
