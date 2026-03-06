/*
 * (C) Copyright 2025, SECO Mind Srl
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef DEVICE_HANDLER_H
#define DEVICE_HANDLER_H

#include <astarte_device_sdk/device.h>

void setup_device(void *data);
void free_device();
// NOTE: this is a highly insecure design pattern. Will be left for compatibility.
// TODO: Improve this API to avoid passing externally the device handle.
astarte_device_handle_t get_device();
void wait_for_device_connection();
void disconnect_device();
void wait_for_device_disconnection();
uint64_t perfect_hash_device_interface(const char *interface_name, size_t len);

#endif /* DEVICE_HANDLER_H */
