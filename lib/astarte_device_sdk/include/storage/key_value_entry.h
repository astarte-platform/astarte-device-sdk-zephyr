/*
 * (C) Copyright 2024, SECO Mind Srl
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef STORAGE_KEY_VALUE_PAIR_H
#define STORAGE_KEY_VALUE_PAIR_H

/**
 * @file storage/key_value_entry.h
 * @brief Helper functions for the key-value persistent storage implementation.
 */

#include <stdbool.h>
#include <stdint.h>

#include "astarte_device_sdk/result.h"

#include <zephyr/version.h>

#if (KERNEL_VERSION_MAJOR >= 4) && (KERNEL_VERSION_MINOR >= 4)
#include <zephyr/kvss/nvs.h>
#else
#include <zephyr/fs/nvs.h>
#endif

/** @brief Number of bytes required to store a version. */
#define ASTARTE_STORAGE_KEY_VALUE_ENTRY_VERSON_LEN_BYTES 3

/**
 * @brief Retrieves the storage driver version from NVS.
 *
 * @param[inout] nvs_fs NVS file system.
 * @param[out] version Pointer to store the retrieved version.
 * @return ASTARTE_RESULT_OK, ASTARTE_RESULT_NOT_FOUND, or error code.
 */
astarte_result_t astarte_storage_key_value_entry_read_version(struct nvs_fs *nvs_fs,
    uint8_t version[static ASTARTE_STORAGE_KEY_VALUE_ENTRY_VERSON_LEN_BYTES]);

/**
 * @brief Writes the storage driver version to NVS.
 *
 * @param[inout] nvs_fs NVS file system.
 * @param[in] version Version to store.
 * @return ASTARTE_RESULT_OK or error code.
 */
astarte_result_t astarte_storage_key_value_entry_write_version(struct nvs_fs *nvs_fs,
    uint8_t version[static ASTARTE_STORAGE_KEY_VALUE_ENTRY_VERSON_LEN_BYTES]);

/**
 * @brief Finds an existing NVS ID via hash and probing, or allocates an available one.
 *
 * @param[inout] nvs_fs NVS file system.
 * @param[in] namespace Target namespace string.
 * @param[in] key Target key string.
 * @param[out] idx Returns the matched or allocated ID.
 * @param[in] allocate True if a new ID should be returned upon not finding the key.
 * @return ASTARTE_RESULT_OK, ASTARTE_RESULT_NOT_FOUND, or error code.
 */
astarte_result_t astarte_storage_key_value_entry_find_or_alloc(
    struct nvs_fs *nvs_fs, const char *namespace, const char *key, uint16_t *idx, bool allocate);

/**
 * @brief Writes an atomic combined payload to the specified ID.
 *
 * @param[inout] nvs_fs NVS file system.
 * @param[in] idx Base NVS ID.
 * @param[in] namespace Target namespace string.
 * @param[in] key Target key string.
 * @param[in] value Target value block.
 * @param[in] value_size Target value block size.
 * @return ASTARTE_RESULT_OK or error code.
 */
astarte_result_t astarte_storage_key_value_entry_write(struct nvs_fs *nvs_fs, uint16_t idx,
    const char *namespace, const char *key, const void *value, size_t value_size);

/**
 * @brief Retrieves a previously stored value from a combined record payload.
 *
 * @param[inout] nvs_fs NVS file system.
 * @param[in] idx Valid NVS ID.
 * @param[out] value Preallocated memory to store the retrieved data. Can be NULL to query size.
 * @param[inout] value_size Pass size of value block, returns data stored.
 * @return ASTARTE_RESULT_OK or error code.
 */
astarte_result_t astarte_storage_key_value_entry_read_value(
    struct nvs_fs *nvs_fs, uint16_t idx, void *value, size_t *value_size);

/**
 * @brief Retrieves a previously stored key string from a combined record payload.
 *
 * @param[inout] nvs_fs NVS file system.
 * @param[in] idx Valid NVS ID.
 * @param[out] key Preallocated memory to store the retrieved string. Can be NULL to query size.
 * @param[inout] key_size Pass size of key block, returns length stored (+1 for null terminator).
 * @return ASTARTE_RESULT_OK or error code.
 */
astarte_result_t astarte_storage_key_value_entry_read_key(
    struct nvs_fs *nvs_fs, uint16_t idx, char *key, size_t *key_size);

/**
 * @brief Evaluates whether a certain NVS ID belongs to a given namespace.
 *
 * @param[inout] nvs_fs NVS file system.
 * @param[in] idx Valid NVS ID to evaluate.
 * @param[in] namespace Namespace to match against.
 * @param[out] matches Evaluated to true if it matches, else false.
 * @return ASTARTE_RESULT_OK or error code.
 */
astarte_result_t astarte_storage_key_value_entry_check_namespace(
    struct nvs_fs *nvs_fs, uint16_t idx, const char *namespace, bool *matches);

/**
 * @brief Deletes a specific key-value entry and repairs the linked list integrity.
 *
 * @param[inout] nvs_fs NVS file system.
 * @param[in] idx Valid NVS ID of the entry to delete.
 * @return ASTARTE_RESULT_OK or error code.
 */
astarte_result_t astarte_storage_key_value_entry_delete(struct nvs_fs *nvs_fs, uint16_t idx);

/**
 * @brief Retrieves the next ID in the linked list of entries.
 *
 * @param[inout] nvs_fs NVS file system.
 * @param[in] idx Valid NVS ID of the current entry, or 0 to retrieve the head ID.
 * @param[out] next_id Pointer to store the retrieved next ID.
 * @return ASTARTE_RESULT_OK or error code.
 */
astarte_result_t astarte_storage_key_value_entry_get_next_id(
    struct nvs_fs *nvs_fs, uint16_t idx, uint16_t *next_id);

#endif // STORAGE_KEY_VALUE_PAIR_H
