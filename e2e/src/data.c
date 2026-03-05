/*
 * (C) Copyright 2026, SECO Mind Srl
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "data.h"

#include <zephyr/logging/log.h>
#include <zephyr/sys/hash_map.h>
#include <zephyr/sys/hash_map_api.h>

#include "device_handler.h"
#include "utilities.h"

/************************************************
 * Constants, static variables and defines
 ***********************************************/

LOG_MODULE_REGISTER(e2e_data, CONFIG_DATA_LOG_LEVEL);

typedef struct
{
    const astarte_interface_t *interface;
} data_map_value_t;

SYS_HASHMAP_DEFINE_STATIC(interface_map);

/************************************************
 * Global functions definition
 ***********************************************/

void data_init(const astarte_interface_t *interfaces[], size_t interfaces_len)
{
    CHECK_HALT(
        !sys_hashmap_is_empty(&interface_map), "Attempting to initialize non empty device data");

    for (size_t i = 0; i < interfaces_len; ++i) {
        uint64_t key
            = perfect_hash_device_interface(interfaces[i]->name, strlen(interfaces[i]->name));
        data_map_value_t *value = malloc(sizeof(data_map_value_t));
        CHECK_HALT(!value, "Could not allocate value required memory");
        value->interface = interfaces[i];

        sys_hashmap_insert(&interface_map, key, POINTER_TO_UINT(value), NULL);
    }
}

const astarte_interface_t *data_get_interface(const char *interface_name, size_t len)
{
    uint64_t key = perfect_hash_device_interface(interface_name, len);
    uint64_t value = { 0 };

    if (sys_hashmap_get(&interface_map, key, &value)) {
        data_map_value_t *map_value = UINT_TO_POINTER(value);
        return map_value->interface;
    }

    return NULL;
}
