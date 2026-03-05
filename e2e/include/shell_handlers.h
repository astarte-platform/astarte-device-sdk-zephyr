/*
 * (C) Copyright 2025, SECO Mind Srl
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef SHELL_HANDLERS_H
#define SHELL_HANDLERS_H

#include <astarte_device_sdk/device.h>

// NOTE: Passing the device handle around like this is highly unsafe
void init_shell(astarte_device_handle_t device_handle, void *data);

#endif /* SHELL_HANDLERS_H */
