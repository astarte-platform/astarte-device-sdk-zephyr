/*
 * (C) Copyright 2024, SECO Mind Srl
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef STORAGE_KEY_VALUE_H
#define STORAGE_KEY_VALUE_H

/**
 * @file storage/key_value.h
 * @brief Key-value persistent storage implementation, with namespacing. Uses NVS as backend.
 *
 * @details
 * Each namespaced key-value pair is stored as a single NVS entry to ensure atomic operations.
 * The entry ID is generated via a CRC16 hash of the concatenated namespace and key strings,
 * with linear probing used to handle collisions.
 *
 * The payload of each NVS entry is structured as follows:
 * - 2 bytes: Namespace string length (excluding null terminator)
 * - 2 bytes: Key string length (excluding null terminator)
 * - N bytes: Namespace string (no null terminator)
 * - K bytes: Key string (no null terminator)
 * - V bytes: Value data
 *
 * This implementation avoids maintaining a central index to drastically reduce flash wear
 * and relies on NVS's built-in atomic guarantees to ensure power-safe operations.
 *
 * This driver implements the following functionalities:
 * - Inserting a key-value pair.
 * - Fetching a value from a known key.
 * - Removing a key-value pair.
 * - Iterating through all the stored key-value pairs.
 */

#include <zephyr/version.h>

#if (KERNEL_VERSION_MAJOR >= 4) && (KERNEL_VERSION_MINOR >= 4)
#include <zephyr/kvss/nvs.h>
#else
#include <zephyr/fs/nvs.h>
#endif

#include "astarte_device_sdk/astarte.h"
#include "astarte_device_sdk/result.h"

/** @brief Configuration struct for a key-value storage instance. */
typedef struct
{
    /** @brief Flash device runtime structure */
    const struct device *flash_device;
    /** @brief Flash partition offset */
    off_t flash_offset;
    /** @brief Full size of the partition */
    uint64_t flash_partition_size;
} astarte_storage_key_value_cfg_t;

/** @brief Data struct for an instance of the key-value storage driver. */
typedef struct
{
    /** @brief Namespace used for this key-value storage instance. */
    char *namespace;
    /** @brief Persistent NVS file system context */
    struct nvs_fs *nvs_fs;
} astarte_storage_key_value_t;

/** @brief Iterator struct for the key-value pair storage. */
typedef struct
{
    /** @brief Reference to the storage instance used by the iterator. */
    astarte_storage_key_value_t *kv_storage;
    /** @brief Current NVS ID pointed to by the iterator. */
    uint16_t current_id;
} astarte_storage_key_value_iter_t;

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize a NVS partition for use with the key-value driver.
 *
 * @note Each NVS partition should be only initialize once. Furthermore call this function only
 * once for each NVS partition.
 *
 * @param[in] config Configuration struct for the NVS partition.
 * @param[in,out] nvs_fs The NVS file system context to open.
 * @return ASTARTE_RESULT_OK if successful, otherwise an error code.
 */
astarte_result_t astarte_storage_key_value_open(
    astarte_storage_key_value_cfg_t config, struct nvs_fs *nvs_fs);

/**
 * @brief Create a new instance of the key-value pairs storage driver for a specific namespace.
 *
 * @note After being used the key-value storage instance should be destroyed with
 * #astarte_storage_key_value_destroy.
 *
 * @param[in,out] nvs_fs The NVS file system context to use. This should have been opened previously
 * using #astarte_storage_key_value_open
 * @param[in] namespace The namespace to be used for this storage instance.
 * @param[out] kv_storage Data struct for the key-value storage instance to initialize.
 * @return ASTARTE_RESULT_OK if successful, otherwise an error code.
 */
astarte_result_t astarte_storage_key_value_new(
    struct nvs_fs *nvs_fs, const char *namespace, astarte_storage_key_value_t *kv_storage);

/**
 * @brief Destroy an instance of the key-value pairs storage driver.
 *
 * @param[inout] kv_storage The storage instance to destroy.
 */
void astarte_storage_key_value_destroy(astarte_storage_key_value_t *kv_storage);

/**
 * @brief Insert or update a new key-value pair into storage.
 *
 * @param[inout] kv_storage Data struct for the instance of the driver.
 * @param[in] key Key to the value to store.
 * @param[in] value Value to store.
 * @param[in] value_size Size of the value to store.
 * @return ASTARTE_RESULT_OK if successful, otherwise an error code.
 */
astarte_result_t astarte_storage_key_value_insert(
    astarte_storage_key_value_t *kv_storage, const char *key, const void *value, size_t value_size);

/**
 * @brief Find an key-value pair matching the @p key in storage.
 *
 * @param[inout] kv_storage Data struct for the instance of the driver.
 * @param[in] key Key to use for the search.
 * @param[out] value Buffer where to store the value for the pair, can be NULL.
 * @param[inout] value_size When @p value is not NULL it should correspond the the size the @p value
 * buffer. Upon success it will be set to the size of the read data or to the required buffer size.
 * @return ASTARTE_RESULT_OK if successful, otherwise an error code.
 */
astarte_result_t astarte_storage_key_value_find(
    astarte_storage_key_value_t *kv_storage, const char *key, void *value, size_t *value_size);

/**
 * @brief Delete an existing key-value pair from storage.
 *
 * @param[inout] kv_storage Data struct for the instance of the driver.
 * @param[in] key Key of the key-value pair to delete.
 * @return ASTARTE_RESULT_OK if successful, otherwise an error code.
 */
astarte_result_t astarte_storage_key_value_delete(
    astarte_storage_key_value_t *kv_storage, const char *key);

/**
 * @brief Initialize a new iterator to be used to iterate over the keys present in storage.
 *
 * @param[in] kv_storage Data struct for the instance of the driver.
 * @param[out] iter Iterator instance to initialize.
 * @return ASTARTE_RESULT_OK if successful, ASTARTE_RESULT_NOT_FOUND if not found, otherwise an
 * error code.
 */
astarte_result_t astarte_storage_key_value_iterator_init(
    astarte_storage_key_value_t *kv_storage, astarte_storage_key_value_iter_t *iter);

/**
 * @brief Advance the iterator of one position.
 *
 * @param[inout] iter Iterator instance.
 * @return ASTARTE_RESULT_OK if successful, ASTARTE_RESULT_NOT_FOUND if not found, otherwise an
 * error code.
 */
astarte_result_t astarte_storage_key_value_iterator_next(astarte_storage_key_value_iter_t *iter);

/**
 * @brief Get the key for the key-valie pair pointed to by the iterator.
 *
 * @param[in] iter Iterator instance.
 * @param[out] key Buffer where to store the key for the pair, can be set to NULL.
 * @param[inout] key_size When @p key is not NULL it should correspond the the size the @p key
 * buffer. Upon success it will be set to the size of the read data or to the required buffer size.
 * @return ASTARTE_RESULT_OK if successful, otherwise an error code.
 */
astarte_result_t astarte_storage_key_value_iterator_get(
    astarte_storage_key_value_iter_t *iter, void *key, size_t *key_size);

#ifdef __cplusplus
}
#endif

#endif // STORAGE_KEY_VALUE_H
