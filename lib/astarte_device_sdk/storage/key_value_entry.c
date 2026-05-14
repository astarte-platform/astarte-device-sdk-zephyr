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

// TODO: This driver is weak to power losses in between writes. Consider introducing a tombstone
// method to be more resilient over power losses.

/************************************************
 *        Defines, constants and typedef        *
 ***********************************************/

#define HEADER_NAMESPACE_LEN_BYTES 2
#define HEADER_KEY_LEN_BYTES 2
#define HEADER_NEXT_ID_BYTES 2
#define HEADER_PREV_ID_BYTES 2
#define FIXED_HEADER_BYTES                                                                         \
    (HEADER_NAMESPACE_LEN_BYTES + HEADER_KEY_LEN_BYTES + HEADER_NEXT_ID_BYTES                      \
        + HEADER_PREV_ID_BYTES)

#define HEAD_AND_TAIL_ID_POSITION (UINT16_MAX - 1)

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
 *
 * @param[inout] nvs_fs NVS file system.
 * @param[in] curr_id The ID of the entry to match.
 * @param[in] namespace Namespace to match.
 * @param[in] key Key to match.
 * @return ASTARTE_RESULT_OK or error code.
 */
static astarte_result_t check_entry_match(
    struct nvs_fs *nvs_fs, uint16_t curr_id, const char *namespace, const char *key);
/**
 * @brief Reads the head and tail IDs of the global linked list.
 *
 * @param[inout] nvs_fs NVS file system.
 * @param[out] head_id New head ID to store.
 * @param[out] tail_id New tail ID to store.
 */
static void read_head_and_tail_ids(struct nvs_fs *nvs_fs, uint16_t *head_id, uint16_t *tail_id);
/**
 * @brief Writes the head and tail IDs of the global linked list.
 *
 * @param[inout] nvs_fs NVS file system.
 * @param[in] head_id New head ID to store.
 * @param[in] tail_id New tail ID to store.
 */
static void write_head_and_tail_ids(struct nvs_fs *nvs_fs, uint16_t head_id, uint16_t tail_id);
/**
 * @brief Updates the next ID pointer of a specific entry in the linked list.
 *
 * @param[inout] nvs_fs NVS file system.
 * @param[in] idx NVS ID of the entry to update.
 * @param[in] new_next New next ID to set.
 * @return ASTARTE_RESULT_OK or error code.
 */
static astarte_result_t update_entry_next_id(
    struct nvs_fs *nvs_fs, uint16_t idx, uint16_t new_next);
/**
 * @brief Updates the previous ID pointer of a specific entry in the linked list.
 *
 * @param[inout] nvs_fs NVS file system.
 * @param[in] idx NVS ID of the entry to update.
 * @param[in] new_prev New previous ID to set.
 * @return ASTARTE_RESULT_OK or error code.
 */
static astarte_result_t update_entry_prev_id(
    struct nvs_fs *nvs_fs, uint16_t idx, uint16_t new_prev);

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
        // Read the fixed header
        uint16_t fixed_header[FIXED_HEADER_BYTES / sizeof(uint16_t)] = { 0 };
        ssize_t ret = nvs_read(nvs_fs, curr_id, fixed_header, FIXED_HEADER_BYTES);

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

        // Check if the key and namespace lengths are correct
        if (fixed_header[0] == nsp_len && fixed_header[1] == key_len) {
            // Check if the namespace and key match the stored values
            astarte_result_t match_res = check_entry_match(nvs_fs, curr_id, namespace, key);
            if (match_res == ASTARTE_RESULT_OK) {
                *idx = curr_id;
                ASTARTE_LOG_DBG("Found matching entry at ID %d", curr_id);
                return ASTARTE_RESULT_OK;
            }
            if (match_res != ASTARTE_RESULT_NOT_FOUND) {
                ASTARTE_LOG_ERR("Error while matching namespace and key at ID: %d", curr_id);
                return match_res;
            }
            // If ASTARTE_RESULT_NOT_FOUND is returned, it means a collision occurred.
            // The loop will continue to the next ID.
        }

        // Increase the ID to handle collisions
        curr_id++;
        if (curr_id == HEAD_AND_TAIL_ID_POSITION) {
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
    uint16_t nsp_len = strlen(namespace);
    uint16_t key_len = strlen(key);
    uint8_t *payload = NULL;
    size_t payload_size = FIXED_HEADER_BYTES + nsp_len + key_len + value_size;

    // Find the next and previous IDs if the entry already exists to maintain list integrity.
    // Otherwise, these will be set to link the new entry at the end of the list.
    uint16_t prev_id = 0;
    uint16_t next_id = 0;

    uint16_t fixed_header[FIXED_HEADER_BYTES / sizeof(uint16_t)] = { 0 };
    ssize_t ret = nvs_read(nvs_fs, idx, fixed_header, FIXED_HEADER_BYTES);
    if (ret == -ENOENT) {
        // The entry does not exists, set prev and next to appropriate values
        uint16_t head_id = 0;
        uint16_t tail_id = 0;
        read_head_and_tail_ids(nvs_fs, &head_id, &tail_id);

        // The previous will always point to the old tail, the next to nothing
        prev_id = tail_id;
        next_id = 0;

        // If there was a tail then update the next value for the old tail with the new one
        // Otherwise the list is empty and this entry is also the head
        if (tail_id != 0) {
            // NOLINTNEXTLINE(readability-suspicious-call-argument)
            update_entry_next_id(nvs_fs, tail_id, idx);
        } else {
            head_id = idx;
        }
        tail_id = idx;
        write_head_and_tail_ids(nvs_fs, head_id, tail_id);
    } else if (ret >= FIXED_HEADER_BYTES) {
        next_id = fixed_header[2];
        prev_id = fixed_header[3];
    } else {
        return ASTARTE_RESULT_NVS_ERROR;
    }

    payload = k_calloc(payload_size, sizeof(uint8_t));
    if (!payload) {
        ASTARTE_LOG_ERR("Out of memory %s: %d", __FILE__, __LINE__);
        ares = ASTARTE_RESULT_OUT_OF_MEMORY;
        goto exit;
    }

    // NOLINTBEGIN(bugprone-not-null-terminated-result)
    size_t write_offset = 0;
    size_t write_size = HEADER_NAMESPACE_LEN_BYTES;
    memcpy(payload + write_offset, &nsp_len, write_size);
    write_offset += write_size;
    write_size = HEADER_KEY_LEN_BYTES;
    memcpy(payload + write_offset, &key_len, write_size);
    write_offset += write_size;
    write_size = HEADER_NEXT_ID_BYTES;
    memcpy(payload + write_offset, &next_id, write_size);
    write_offset += write_size;
    write_size = HEADER_PREV_ID_BYTES;
    memcpy(payload + write_offset, &prev_id, write_size);
    write_offset += write_size;
    write_size = nsp_len;
    memcpy(payload + write_offset, namespace, write_size);
    write_offset += write_size;
    write_size = key_len;
    memcpy(payload + write_offset, key, write_size);
    if (value_size > 0 && value != NULL) {
        write_offset += write_size;
        write_size = value_size;
        memcpy(payload + write_offset, value, write_size);
    }
    // NOLINTEND(bugprone-not-null-terminated-result)

    ret = nvs_write(nvs_fs, idx, payload, payload_size);
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
    uint8_t *payload = NULL;

    // Figure out the full payload size
    ssize_t payload_size = nvs_read(nvs_fs, idx, NULL, 0);
    if (payload_size < 0) {
        ASTARTE_LOG_ERR("Error reading from NVS at ID %d, error: %d", idx, payload_size);
        ares = ASTARTE_RESULT_NVS_ERROR;
        goto exit;
    }

    // Read the fixed header
    uint16_t fixed_header[FIXED_HEADER_BYTES / sizeof(uint16_t)] = { 0 };
    ssize_t ret = nvs_read(nvs_fs, idx, fixed_header, FIXED_HEADER_BYTES);
    if (ret < 0) {
        ASTARTE_LOG_ERR("Error reading fixed_header from NVS at ID %d, error: %d", idx, ret);
        ares = ASTARTE_RESULT_NVS_ERROR;
        goto exit;
    }

    // Calculate the full header size
    size_t header_size = FIXED_HEADER_BYTES + fixed_header[0] + fixed_header[1];
    if ((size_t) payload_size < header_size) {
        ASTARTE_LOG_ERR("Error: Incomplete header at ID %d", idx);
        ares = ASTARTE_RESULT_INTERNAL_ERROR;
        goto exit;
    }

    // Compute the value size and return it immediately if there is no output buffer
    size_t read_value_size = payload_size - header_size;
    if (!value) {
        *value_size = read_value_size;
        goto exit;
    }
    if (*value_size < read_value_size) {
        ASTARTE_LOG_ERR("Error: Value buffer too small at ID %d", idx);
        ares = ASTARTE_RESULT_INVALID_PARAM;
        goto exit;
    }

    // Read the full entry
    payload = k_calloc(payload_size, sizeof(uint8_t));
    if (!payload) {
        ASTARTE_LOG_ERR("Out of memory %s: %d", __FILE__, __LINE__);
        ares = ASTARTE_RESULT_OUT_OF_MEMORY;
        goto exit;
    }
    ret = nvs_read(nvs_fs, idx, payload, payload_size);
    if (ret < 0) {
        ASTARTE_LOG_ERR("Error reading full payload from NVS at ID %d, error: %d", idx, ret);
        ares = ASTARTE_RESULT_NVS_ERROR;
        goto exit;
    }

    if (read_value_size > 0) {
        memcpy(value, payload + header_size, read_value_size);
    }

    *value_size = read_value_size;

exit:
    k_free(payload);
    return ares;
}

astarte_result_t astarte_storage_key_value_entry_read_key(
    struct nvs_fs *nvs_fs, uint16_t idx, char *key, size_t *key_size)
{
    astarte_result_t ares = ASTARTE_RESULT_OK;
    uint8_t *header = NULL;

    uint16_t fixed_header[FIXED_HEADER_BYTES / sizeof(uint16_t)] = { 0 };
    ssize_t ret = nvs_read(nvs_fs, idx, fixed_header, FIXED_HEADER_BYTES);
    if (ret < 0) {
        ASTARTE_LOG_ERR("Error reading header from NVS at ID %d, error: %d", idx, ret);
        ares = ASTARTE_RESULT_NVS_ERROR;
        goto exit;
    }

    size_t nsp_len = fixed_header[0];
    size_t key_len = fixed_header[1];

    if (!key) {
        // +1 includes room for termination
        *key_size = key_len + 1;
        goto exit;
    }

    if (*key_size < key_len + 1) {
        ASTARTE_LOG_ERR("Error: Key buffer too small at ID %d", idx);
        ares = ASTARTE_RESULT_INVALID_PARAM;
        goto exit;
    }

    size_t header_size = FIXED_HEADER_BYTES + nsp_len + key_len;
    header = k_calloc(header_size, sizeof(uint8_t));
    if (!header) {
        ASTARTE_LOG_ERR("Out of memory %s: %d", __FILE__, __LINE__);
        ares = ASTARTE_RESULT_OUT_OF_MEMORY;
        goto exit;
    }

    ret = nvs_read(nvs_fs, idx, header, header_size);
    if (ret < 0) {
        ASTARTE_LOG_ERR("Error reading header from NVS at ID %d, error: %d", idx, ret);
        ares = ASTARTE_RESULT_NVS_ERROR;
        goto exit;
    }

    memcpy(key, header + FIXED_HEADER_BYTES + nsp_len, key_len);
    key[key_len] = '\0';
    *key_size = key_len + 1;

exit:
    k_free(header);
    return ares;
}

astarte_result_t astarte_storage_key_value_entry_check_namespace(
    struct nvs_fs *nvs_fs, uint16_t idx, const char *namespace, bool *matches)
{
    astarte_result_t ares = ASTARTE_RESULT_OK;
    uint8_t *partial_header = NULL;

    uint16_t fixed_header[FIXED_HEADER_BYTES / sizeof(uint16_t)] = { 0 };
    ssize_t ret = nvs_read(nvs_fs, idx, fixed_header, FIXED_HEADER_BYTES);

    if (ret == -ENOENT) {
        *matches = false;
        goto exit;
    } else if (ret < 0) {
        ASTARTE_LOG_ERR("Error reading header from NVS at ID %d, error: %d", idx, ret);
        ares = ASTARTE_RESULT_NVS_ERROR;
        goto exit;
    }

    size_t nsp_len = fixed_header[0];
    if (nsp_len != strlen(namespace)) {
        *matches = false;
        goto exit;
    }

    size_t partial_header_size = FIXED_HEADER_BYTES + nsp_len;
    partial_header = k_calloc(partial_header_size, sizeof(uint8_t));
    if (!partial_header) {
        ASTARTE_LOG_ERR("Out of memory %s: %d", __FILE__, __LINE__);
        ares = ASTARTE_RESULT_OUT_OF_MEMORY;
        goto exit;
    }

    ret = nvs_read(nvs_fs, idx, partial_header, partial_header_size);
    if (ret < 0) {
        ASTARTE_LOG_ERR("Error reading header from NVS at ID %d, error: %d", idx, ret);
        ares = ASTARTE_RESULT_NVS_ERROR;
        goto exit;
    }

    int nsp_cmp = strncmp((char *) partial_header + FIXED_HEADER_BYTES, namespace, nsp_len);
    if (nsp_cmp == 0) {
        *matches = true;
    } else {
        *matches = false;
    }

exit:
    k_free(partial_header);
    return ares;
}

astarte_result_t astarte_storage_key_value_entry_delete(struct nvs_fs *nvs_fs, uint16_t idx)
{
    uint16_t fixed_header[FIXED_HEADER_BYTES / sizeof(uint16_t)] = { 0 };
    ssize_t ret = nvs_read(nvs_fs, idx, fixed_header, FIXED_HEADER_BYTES);
    if (ret == -ENOENT) {
        return ASTARTE_RESULT_OK;
    }
    if (ret < 0) {
        return ASTARTE_RESULT_NVS_ERROR;
    }

    // Fetch next and previous entries as well as head and tail
    uint16_t next_id = fixed_header[2];
    uint16_t prev_id = fixed_header[3];

    uint16_t head_id = 0;
    uint16_t tail_id = 0;
    read_head_and_tail_ids(nvs_fs, &head_id, &tail_id);

    // Update the previous entry next id to the next one of the entry to delete
    // Or if this entry is the head set the head to the next entry
    if (prev_id != 0) {
        update_entry_next_id(nvs_fs, prev_id, next_id);
    } else {
        head_id = next_id;
    }

    // Update the next entry previous id to the previous one of the entry to delete
    // Or if this node is the tail set the tail to the previous entry
    if (next_id != 0) {
        update_entry_prev_id(nvs_fs, next_id, prev_id);
    } else {
        tail_id = prev_id;
    }

    // Update head and tail to the new values
    write_head_and_tail_ids(nvs_fs, head_id, tail_id);

    // Delete the entry
    ret = nvs_delete(nvs_fs, idx);
    if (ret < 0 && ret != -ENOENT) {
        return ASTARTE_RESULT_NVS_ERROR;
    }

    return ASTARTE_RESULT_OK;
}

astarte_result_t astarte_storage_key_value_entry_get_next_id(
    struct nvs_fs *nvs_fs, uint16_t idx, uint16_t *next_id)
{
    // Calling this function with ID 0 will make it return the ID of the head
    if (idx == 0) {
        uint16_t head_id = 0;
        uint16_t tail_id = 0;
        read_head_and_tail_ids(nvs_fs, &head_id, &tail_id);
        *next_id = head_id;
        return ASTARTE_RESULT_OK;
    }

    uint16_t fixed_header[FIXED_HEADER_BYTES / sizeof(uint16_t)] = { 0 };
    ssize_t ret = nvs_read(nvs_fs, idx, fixed_header, FIXED_HEADER_BYTES);
    if (ret < 0) {
        return ASTARTE_RESULT_NVS_ERROR;
    }
    *next_id = fixed_header[2];
    return ASTARTE_RESULT_OK;
}

/************************************************
 *         Static functions definitions         *
 ***********************************************/

static void read_head_and_tail_ids(struct nvs_fs *nvs_fs, uint16_t *head_id, uint16_t *tail_id)
{
    uint16_t ids[2] = { 0 };
    ssize_t ret = nvs_read(nvs_fs, HEAD_AND_TAIL_ID_POSITION, ids, sizeof(ids));
    if (ret < 0) {
        *head_id = 0;
        *tail_id = 0;
    } else {
        *head_id = ids[0];
        *tail_id = ids[1];
    }
}

static void write_head_and_tail_ids(struct nvs_fs *nvs_fs, uint16_t head_id, uint16_t tail_id)
{
    uint16_t ids[2] = { head_id, tail_id };
    // TODO: evaluate if this write is a success
    nvs_write(nvs_fs, HEAD_AND_TAIL_ID_POSITION, ids, sizeof(ids));
}

static astarte_result_t update_entry_next_id(struct nvs_fs *nvs_fs, uint16_t idx, uint16_t new_next)
{
    ssize_t payload_size = nvs_read(nvs_fs, idx, NULL, 0);
    if (payload_size <= 0) {
        return ASTARTE_RESULT_NVS_ERROR;
    }

    uint8_t *payload = k_calloc(payload_size, 1);
    if (!payload) {
        return ASTARTE_RESULT_OUT_OF_MEMORY;
    }

    // TODO: evaluate if this read is a success
    nvs_read(nvs_fs, idx, payload, payload_size);
    ((uint16_t *) payload)[2] = new_next;
    // TODO: evaluate if this write is a success
    nvs_write(nvs_fs, idx, payload, payload_size);

    k_free(payload);
    return ASTARTE_RESULT_OK;
}

static astarte_result_t update_entry_prev_id(struct nvs_fs *nvs_fs, uint16_t idx, uint16_t new_prev)
{
    ssize_t payload_size = nvs_read(nvs_fs, idx, NULL, 0);
    if (payload_size <= 0) {
        return ASTARTE_RESULT_NVS_ERROR;
    }

    uint8_t *payload = k_calloc(payload_size, 1);
    if (!payload) {
        return ASTARTE_RESULT_OUT_OF_MEMORY;
    }

    // TODO: evaluate if this read is a success
    nvs_read(nvs_fs, idx, payload, payload_size);
    ((uint16_t *) payload)[3] = new_prev;
    // TODO: evaluate if this write is a success
    nvs_write(nvs_fs, idx, payload, payload_size);

    k_free(payload);
    return ASTARTE_RESULT_OK;
}

static uint16_t generate_hash(const char *namespace, const char *key)
{
    uint16_t crc = UINT16_MAX; // Zephyr's crc16_ccitt seed
    crc = crc16_ccitt(crc, (const uint8_t *) namespace, strlen(namespace));
    crc = crc16_ccitt(crc, (const uint8_t *) key, strlen(key));

    // Fallbacks to avoid invalid Zephyr NVS IDs or our reserved list ptrs block (0xFFFE)
    if (crc == UINT16_MAX || crc == 0 || crc == HEAD_AND_TAIL_ID_POSITION) {
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
    size_t header_size = FIXED_HEADER_BYTES + nsp_len + key_len;

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
        int nsp_cmp = strncmp((char *) header + FIXED_HEADER_BYTES, namespace, nsp_len);
        int key_cmp = strncmp((char *) header + FIXED_HEADER_BYTES + nsp_len, key, key_len);
        if (nsp_cmp == 0 && key_cmp == 0) {
            ares = ASTARTE_RESULT_OK;
            goto exit;
        }
    }

exit:
    k_free(header);
    return ares;
}
