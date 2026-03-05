/*
 * (C) Copyright 2024, SECO Mind Srl
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef DATA_H
#define DATA_H

#include <astarte_device_sdk/interface.h>

void data_init(const astarte_interface_t *interfaces[], size_t interfaces_len);
const astarte_interface_t *data_get_interface(const char *interface_name, size_t len);

#endif // DATA_H
