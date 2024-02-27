#include "sntp_sync.h"

#include <zephyr/net/sntp.h>
#include <zephyr/posix/time.h>

#define SNTP_SERVER "0.pool.ntp.org"

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(sntp_sync, CONFIG_APP_LOG_LEVEL); // NOLINT

int sntp_sync_init(void)
{
    int rc;
    struct sntp_time now;
    struct timespec tspec;

    rc = sntp_simple(SNTP_SERVER, SYS_FOREVER_MS, &now);
    if (rc == 0) {
        tspec.tv_sec = now.seconds;
        tspec.tv_nsec = ((uint64_t) now.fraction * (1000lu * 1000lu * 1000lu)) >> 32;

        clock_settime(CLOCK_REALTIME, &tspec);

        LOG_DBG("Acquired time from NTP server: %u", (uint32_t) tspec.tv_sec);
    } else {
        LOG_ERR("Failed to acquire SNTP, code %d\n", rc);
    }
    return rc;
}
