#include "ota.h"
#include "globals.h"
#include "udp.h"
// #include "mdf_common.h"


// copied from mupgrade_example and slightly modified. TODO: integrate better into root node
static void ota_task(const char *upgrade_url)
{
    mdf_err_t ret       = MDF_OK;
    uint8_t *data       = MDF_MALLOC(MWIFI_PAYLOAD_LEN);
    char name[32]       = {0x0};
    int total_size   = 0;
    int start_time      = 0;
    mupgrade_result_t upgrade_result = {0};
    mwifi_data_type_t data_type = {.communicate = MWIFI_COMMUNICATE_MULTICAST};

    /**
     * @note If you need to upgrade all devices, pass MWIFI_ADDR_ANY;
     *       If you upgrade the incoming address list to the specified device
     */
    // uint8_t dest_addr[][MWIFI_ADDR_LEN] = {{0x1, 0x1, 0x1, 0x1, 0x1, 0x1}, {0x2, 0x2, 0x2, 0x2, 0x2, 0x2},};
    uint8_t dest_addr[][MWIFI_ADDR_LEN] = {MWIFI_ADDR_ANY};

    /**
     * @brief In order to allow more nodes to join the mesh network for firmware upgrade,
     *      in the example we will start the firmware upgrade after 30 seconds.
     */
    // vTaskDelay(10 * 1000 / portTICK_PERIOD_MS);

    esp_http_client_config_t config = {
        .url            = upgrade_url,
        .transport_type = HTTP_TRANSPORT_UNKNOWN,
    };

    /**
     * @brief 1. Connect to the server
     */
    esp_http_client_handle_t client = esp_http_client_init(&config);
    MDF_ERROR_GOTO(!client, EXIT, "Initialise HTTP connection");

    start_time = xTaskGetTickCount();

    MDF_LOGI("Open HTTP connection: %s", upgrade_url);

    /**
     * @brief First, the firmware is obtained from the http server and stored on the root node.
     */
    do {
        ret = esp_http_client_open(client, 0);

        if (ret != MDF_OK) {
            if (!esp_mesh_is_root()) {
                goto EXIT;
            }

            vTaskDelay(pdMS_TO_TICKS(1000));
            MDF_LOGW("<%s> Connection service failed", mdf_err_to_name(ret));
        }
    } while (ret != MDF_OK);

    total_size = esp_http_client_fetch_headers(client);
    sscanf(upgrade_url, "%*[^/]//%*[^/]/%[^.]", name);

    if (total_size <= 0) {
        MDF_LOGW("Please check the address of the server");
        ret = esp_http_client_read(client, (char *)data, MWIFI_PAYLOAD_LEN);
        MDF_ERROR_GOTO(ret < 0, EXIT, "<%s> Read data from http stream", mdf_err_to_name(ret));

        MDF_LOGW("Recv data: %.*s", ret, data);
        goto EXIT;
    }

    /**
     * @brief 2. Initialize the upgrade status and erase the upgrade partition.
     */
    ret = mupgrade_firmware_init(name, total_size);
    MDF_ERROR_GOTO(ret != MDF_OK, EXIT, "<%s> Initialize the upgrade status", mdf_err_to_name(ret));

    /**
     * @brief 3. Read firmware from the server and write it to the flash of the root node
     */
    for (ssize_t size = 0, recv_size = 0; recv_size < total_size; recv_size += size) {
        size = esp_http_client_read(client, (char *)data, MWIFI_PAYLOAD_LEN);
        MDF_ERROR_GOTO(size < 0, EXIT, "<%s> Read data from http stream", mdf_err_to_name(ret));

        if (size > 0) {
            /* @brief  Write firmware to flash */
            ret = mupgrade_firmware_download(data, size);
            MDF_ERROR_GOTO(ret != MDF_OK, EXIT, "<%s> Write firmware to flash, size: %d, data: %.*s",
                           mdf_err_to_name(ret), size, size, data);
        } else {
            MDF_LOGW("<%s> esp_http_client_read", mdf_err_to_name(ret));
            goto EXIT;
        }
    }

    MDF_LOGI("The service download firmware is complete, Spend time: %ds",
             (xTaskGetTickCount() - start_time) * portTICK_RATE_MS / 1000);

    start_time = xTaskGetTickCount();

    /**
     * @brief 4. The firmware will be sent to each node.
     */
    ret = mupgrade_firmware_send((uint8_t *)dest_addr, sizeof(dest_addr) / MWIFI_ADDR_LEN, &upgrade_result);
    MDF_ERROR_GOTO(ret != MDF_OK, EXIT, "<%s> mupgrade_firmware_send", mdf_err_to_name(ret));

    if (upgrade_result.successed_num == 0) {
        MDF_LOGW("Devices upgrade failed, unfinished_num: %d", upgrade_result.unfinished_num);
        goto EXIT;
    }

    MDF_LOGI("Firmware is sent to the device to complete, Spend time: %ds",
             (xTaskGetTickCount() - start_time) * portTICK_RATE_MS / 1000);
    MDF_LOGI("Devices upgrade completed, successed_num: %d, unfinished_num: %d", upgrade_result.successed_num, upgrade_result.unfinished_num);

    /**
     * @brief 5. the root notifies nodes to restart
     */
    // const char *restart_str = "restart";
    data_type.custom = CMD_Restart;
    uint8_t dummy = 0;
    ret = mwifi_root_write(upgrade_result.successed_addr, upgrade_result.successed_num,
                           &data_type, &dummy, sizeof(dummy), true);
    MDF_ERROR_GOTO(ret != MDF_OK, EXIT, "<%s> mwifi_root_recv", mdf_err_to_name(ret));

EXIT:
    MDF_FREE(data);
    mupgrade_result_free(&upgrade_result);
    esp_http_client_close(client);
    esp_http_client_cleanup(client);
    vTaskDelete(NULL);
}

void otaHandleEvent(mdf_event_loop_t event, void *ctx) {
    switch(event) {
        case MDF_EVENT_MUPGRADE_STARTED: {
            mupgrade_status_t status = {0x0};
            mupgrade_get_status(&status);

            MDF_LOGI("MDF_EVENT_MUPGRADE_STARTED, name: %s, size: %d",
                     status.name, status.total_size);
            break;
        }

        case MDF_EVENT_MUPGRADE_STATUS:
            MDF_LOGI("Upgrade progress: %d%%", (int)ctx);
            break;
    }
}

void otaStart(const char *upgrade_url) {
    // TODO: also handle upgrade target, currently hardcoded to broadcast
    xTaskCreate(ota_task, "ota_task", 4 * 1024, upgrade_url, CONFIG_MDF_TASK_DEFAULT_PRIOTY, NULL);
}