/*
 * (C) Copyright 2024, SECO Mind Srl
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "backoff.h"

#include <zephyr/random/random.h>

void backoff_context_init(struct backoff_context *ctx, uint32_t base_backoff_ms,
    uint32_t max_backoff_ms, bool enable_jitter)
{
    ctx->attempt_max_backoff_ms = base_backoff_ms;
    ctx->max_backoff_ms = max_backoff_ms;
    ctx->enable_jitter = enable_jitter;
}

void backoff_get_next(struct backoff_context *ctx, uint32_t *next_backoff_ms)
{
    if (ctx->enable_jitter) {
        *next_backoff_ms = sys_rand32_get() % (ctx->attempt_max_backoff_ms + 1U);
    } else {
        *next_backoff_ms = ctx->attempt_max_backoff_ms;
    }

    /* Calculate max backoff for the next attempt (~ 2**attempt) */
    ctx->attempt_max_backoff_ms = MIN(ctx->attempt_max_backoff_ms * 2U, ctx->max_backoff_ms);
}
