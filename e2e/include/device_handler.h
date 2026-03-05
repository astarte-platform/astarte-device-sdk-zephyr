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

// Used only as a token to avoid
typedef void *test_device_handle_t;

void device_setup(astarte_device_config_t config);
// these functions read device_thread_flags and wait appropriately
// flag DEVICE_CONNECTED
void wait_for_connection();
void wait_for_disconnection();
// these functions write device_thread_flags
// flag THREAD_TERMINATION
void set_termination();
// void free_device();
// --

#endif /* DEVICE_HANDLER_H */
