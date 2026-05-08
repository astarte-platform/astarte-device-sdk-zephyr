/*
 * (C) Copyright 2024, SECO Mind Srl
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef STORAGE_KEY_VALUE_PAIR_H
#define STORAGE_KEY_VALUE_PAIR_H

/**
 * @file storage/key_value_pair.h
 * @brief Helper functions for the key-value persistent storage implementation.
 */

#include "astarte_device_sdk/result.h"

#include <zephyr/version.h>

#if (KERNEL_VERSION_MAJOR >= 4) && (KERNEL_VERSION_MINOR >= 4)
#include <zephyr/kvss/nvs.h>
#else
#include <zephyr/fs/nvs.h>
#endif

/**
 * @brief Calculate the base NVS ID for a given pair index.
 *
 * @param[in] pair_index The 0-based index of the pair (0 to stored_pairs - 1).
 * @return The NVS ID for the start of that pair's record.
 */
uint16_t astarte_storage_key_value_pair_get_pair_base_id(uint16_t pair_index);
/**
 * @brief Calculate the base NVS ID for a new pair, safely checking for overflow.
 *
 * @param[in] pair_index The 0-based index of the pair to insert (0 to stored_pairs - 1).
 * @param[out] base_id Upon success, contains the valid base NVS ID.
 * @return ASTARTE_RESULT_OK if successful, ASTARTE_RESULT_KV_STORAGE_FULL if IDs would overflow.
 */
astarte_result_t astarte_storage_key_value_pair_get_new_base_id(
    uint16_t pair_index, uint16_t *base_id);
/**
 * @brief Get number of stored pairs.
 *
 * @param[inout] nvs_fs NVS file system from which to fetch the number.
 * @param[out] count Number of pairs stored in NVS.
 * @return ASTARTE_RESULT_OK if successful, otherwise an error code.
 */
astarte_result_t astarte_storage_key_value_pair_get_pairs_number(
    struct nvs_fs *nvs_fs, uint16_t *count);
/**
 * @brief Update the number of stored pairs.
 *
 * @param[inout] nvs_fs NVS file system in which to store the entry.
 * @param[in] count Updated number of pairs stored in NVS.
 * @return ASTARTE_RESULT_OK if successful, otherwise an error code.
 */
astarte_result_t astarte_storage_key_value_pair_set_pairs_number(
    struct nvs_fs *nvs_fs, uint16_t count);
/**
 * @brief Get the namespace for the NVS pair at the provided base ID.
 *
 * @param[inout] nvs_fs NVS file system from which to fetch the pair.
 * @param[in] base_id NVS base ID for the pair to read.
 * @param[out] namespace Buffer where to store the NVS namespace, can be NULL.
 * @param[inout] namespace_size When @p namespace is non NULL should be the size of @p namespace.
 * Upon success it will be set to the required size to store the namespace.
 * @return ASTARTE_RESULT_OK if successful, otherwise an error code.
 */
astarte_result_t astarte_storage_key_value_pair_read_namespace(
    struct nvs_fs *nvs_fs, uint16_t base_id, char *namespace, size_t *namespace_size);
/**
 * @brief Get the key for the NVS pair at the provided base ID.
 *
 * @param[inout] nvs_fs NVS file system from which to fetch the pair.
 * @param[in] base_id NVS base ID for the pair to read.
 * @param[out] key Buffer where to store the NVS key, can be NULL.
 * @param[inout] key_size When @p key is non NULL should be the size of @p key.
 * Upon success it will be set to the required size to store the key.
 * @return ASTARTE_RESULT_OK if successful, otherwise an error code.
 */
astarte_result_t astarte_storage_key_value_pair_read_key(
    struct nvs_fs *nvs_fs, uint16_t base_id, char *key, size_t *key_size);
/**
 * @brief Get the value for the NVS pair at the provided base ID.
 *
 * @param[inout] nvs_fs NVS file system from which to fetch the pair.
 * @param[in] base_id NVS base ID for the pair to read.
 * @param[out] value Buffer where to store the NVS value, can be NULL.
 * @param[inout] value_size When @p value is non NULL should be the size of @p value.
 * Upon success it will be set to the required size to store the value.
 * @return ASTARTE_RESULT_OK if successful, otherwise an error code.
 */
astarte_result_t astarte_storage_key_value_pair_read_value(
    struct nvs_fs *nvs_fs, uint16_t base_id, void *value, size_t *value_size);
/**
 * @brief Write a key-value pair using an NVS base ID.
 *
 * @param[inout] nvs_fs NVS file system to use.
 * @param[in] base_id Base NVS ID where to store the key-value pair.
 * @param[in] namespace Namespace for the key-value pair.
 * @param[in] key Key for the key-value pair.
 * @param[in] value Value for the key-value pair.
 * @param[in] value_size Size of the value array for the key-value pair.
 * @return ASTARTE_RESULT_OK if successful, otherwise an error code.
 */
astarte_result_t astarte_storage_key_value_pair_write_pair(struct nvs_fs *nvs_fs, uint16_t base_id,
    const char *namespace, const char *key, const void *value, size_t value_size);
/**
 * @brief Relocate a single key-value pair.
 *
 * @param[inout] nvs_fs NVS file system to use.
 * @param[in] dst_base_id Destination NVS base ID where the pair should be relocated.
 * @param[in] src_base_id Source NVS base ID where the pair should be relocated.
 * @return ASTARTE_RESULT_OK if successful, otherwise an error code.
 */
astarte_result_t astarte_storage_key_value_pair_relocate_pair(
    struct nvs_fs *nvs_fs, uint16_t dst_base_id, uint16_t src_base_id);
/**
 * @brief Scans storage for duplicate keys caused by interrupted delete operations.
 *
 * @param[in,out] nvs_fs NVS file system to use.
 * @return ASTARTE_RESULT_OK if successful, otherwise an error code.
 */
astarte_result_t astarte_storage_key_value_pair_remove_duplicates(struct nvs_fs *nvs_fs);

#endif // STORAGE_KEY_VALUE_PAIR_H
