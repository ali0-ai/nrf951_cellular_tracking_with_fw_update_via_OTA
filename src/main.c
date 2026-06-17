/*
 * Copyright (c) 2023 Golioth, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(fw_update_sample, LOG_LEVEL_DBG);

#include <golioth/client.h>
#include <golioth/fw_update.h>
#include <golioth/location/cellular.h>
#include <samples/common/sample_credentials.h>
#include <string.h>
#include <zephyr/kernel.h>

#include <samples/common/net_connect.h>

#include <app_version.h>

#include "cellular.h"

#define APP_TIMEOUT_S 5



// Current firmware version; update in VERSION
// Current firmware version; update in VERSION
static const char *_current_version =
    STRINGIFY(APP_VERSION_MAJOR) "." STRINGIFY(APP_VERSION_MINOR) "." STRINGIFY(APP_PATCHLEVEL);

static K_SEM_DEFINE(connected, 0, 1);

static void on_client_event(struct golioth_client *client,
                            enum golioth_client_event event,
                            void *arg)
{
    bool is_connected = (event == GOLIOTH_CLIENT_EVENT_CONNECTED);
    if (is_connected)
    {
        k_sem_give(&connected);
    }
    LOG_INF("Golioth client %s", is_connected ? "connected" : "disconnected");
}


static struct golioth_location_req location_req;

static int cellular_get_and_encode_info(void)
{
    struct golioth_cellular_info cellular_infos[4];
    enum golioth_status status;
    size_t num_infos;
    int err;

    if (!IS_ENABLED(CONFIG_GOLIOTH_LOCATION_CELLULAR))
    {
        return 0;
    }

    err = cellular_info_get(cellular_infos, ARRAY_SIZE(cellular_infos), &num_infos);
    if (err)
    {
        LOG_ERR("Failed to get cellular network info: %d", err);
        return err;
    }

LOG_INF("Cellular infos found: %d", num_infos);

    for (size_t i = 0; i < num_infos; i++)
    {
        status = golioth_location_cellular_append(&location_req, &cellular_infos[i]);
        if (status != GOLIOTH_OK)
        {
            LOG_ERR("Failed to append cellular info: %d", status);
            return -ENOMEM;
        }
    }


    return 0;
}
int main(void)
{
        struct golioth_location_rsp location_rsp;
    enum golioth_status status;
    int err;

    LOG_DBG("Start location sample");
    LOG_DBG("Start FW Update sample");

    net_connect();

    /* Note: In production, you would provision unique credentials onto each
     * device. For simplicity, we provide a utility to hardcode credentials as
     * kconfig options in the samples.
     */
    const struct golioth_client_config *client_config = golioth_sample_credentials_get();

    struct golioth_client *client = golioth_client_create(client_config);

    golioth_client_register_event_callback(client, on_client_event, NULL);

    golioth_fw_update_init(client, _current_version);

    k_sem_take(&connected, K_FOREVER);

   // send_device_data();

    /* No while(true) loop needed, the Golioth client thread will handle updates */
  while (true)
    {
        golioth_location_init(&location_req);

        err = cellular_get_and_encode_info();
        if (err)
        {
            continue;
        }

        status = golioth_location_finish(&location_req);
        if (status != GOLIOTH_OK)
        {
            if (status == GOLIOTH_ERR_NULL)
            {
                LOG_WRN("No location data to be provided");
                return 0;
            }

            LOG_ERR("Failed to encode location data");
            return -ENOMEM;
        }

        status = golioth_location_get_sync(client, &location_req, &location_rsp, APP_TIMEOUT_S);
        if (status == GOLIOTH_OK)
        {
            LOG_INF("%s%lld.%09lld %s%lld.%09lld (%lld)",
                    location_rsp.latitude < 0 ? "-" : "",
                    llabs(location_rsp.latitude) / 1000000000,
                    llabs(location_rsp.latitude) % 1000000000,
                    location_rsp.longitude < 0 ? "-" : "",
                    llabs(location_rsp.longitude) / 1000000000,
                    llabs(location_rsp.longitude) % 1000000000,
                    (long long int) location_rsp.accuracy);
        }

        k_msleep(CONFIG_APP_LOCATION_GET_INTERVAL);
    }

    return 0;
}
