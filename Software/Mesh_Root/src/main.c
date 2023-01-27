// Copyright 2017 Espressif Systems (Shanghai) PTE LTD
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at

//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "mdf_common.h"
#include "mwifi.h"

#include "secrets.h"
#include "globals.h"
#include "udp.h"
#include "ota.h"
#include "esp_http_client.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_event.h"
#include "esp_netif.h"

#include "cJSON.h"

#define MAX_HTTP_RECV_BUFFER 512
#define MAX_HTTP_OUTPUT_BUFFER 2048

static const char *TAGjson = "JSON";

// #define MEMORY_DEBUG

esp_netif_t *sta_netif;

esp_err_t _http_event_handler(esp_http_client_event_t *evt)
{
    static char *output_buffer; // Buffer to store response of http request from event handler
    static int output_len;      // Stores number of bytes read
    switch (evt->event_id)
    {
    case HTTP_EVENT_ERROR:
        ESP_LOGD(TAG, "HTTP_EVENT_ERROR");
        break;
    case HTTP_EVENT_ON_CONNECTED:
        ESP_LOGD(TAG, "HTTP_EVENT_ON_CONNECTED");
        break;
    case HTTP_EVENT_HEADER_SENT:
        ESP_LOGD(TAG, "HTTP_EVENT_HEADER_SENT");
        break;
    case HTTP_EVENT_ON_HEADER:
        ESP_LOGD(TAG, "HTTP_EVENT_ON_HEADER, key=%s, value=%s", evt->header_key, evt->header_value);
        break;
    case HTTP_EVENT_ON_DATA:
        ESP_LOGD(TAG, "HTTP_EVENT_ON_DATA, len=%d", evt->data_len);
        /*
         *  Check for chunked encoding is added as the URL for chunked encoding used in this example returns binary data.
         *  However, event handler can also be used in case chunked encoding is used.
         */
        if (!esp_http_client_is_chunked_response(evt->client))
        {
            // If user_data buffer is configured, copy the response into the buffer
            if (evt->user_data)
            {
                memcpy(evt->user_data + output_len, evt->data, evt->data_len);
            }
            else
            {
                if (output_buffer == NULL)
                {
                    output_buffer = (char *)malloc(esp_http_client_get_content_length(evt->client));
                    output_len = 0;
                    if (output_buffer == NULL)
                    {
                        ESP_LOGE(TAG, "Failed to allocate memory for output buffer");
                        return ESP_FAIL;
                    }
                }
                memcpy(output_buffer + output_len, evt->data, evt->data_len);
            }
            output_len += evt->data_len;
        }

        break;
    case HTTP_EVENT_ON_FINISH:
        ESP_LOGD(TAG, "HTTP_EVENT_ON_FINISH");
        if (output_buffer != NULL)
        {
            // Response is accumulated in output_buffer. Uncomment the below line to print the accumulated response
            // ESP_LOG_BUFFER_HEX(TAG, output_buffer, output_len);
            free(output_buffer);
            output_buffer = NULL;
        }
        output_len = 0;
        break;
    case HTTP_EVENT_DISCONNECTED:
        ESP_LOGI(TAG, "HTTP_EVENT_DISCONNECTED");
        break;
    }
    return ESP_OK;
}

static void post_rest_function(char *data, char *macAdresse)
{
    esp_http_client_config_t config = {
        .url = "http://192.168.43.173:8080/sendData",
        .method = HTTP_METHOD_POST,
        .event_handler = _http_event_handler};

    esp_http_client_handle_t client = esp_http_client_init(&config);

    // const char *post_data = data;

    ESP_LOGI(TAGjson, "Serialize.....");
    cJSON *root;
    root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "macAdresse", macAdresse);
    cJSON_AddStringToObject(root, "values", data);
    char *my_json_string = cJSON_Print(root);
    ESP_LOGI(TAGjson, "my_json_string\n%s", my_json_string);
    cJSON_Delete(root);

    esp_http_client_set_post_field(client, my_json_string, strlen(my_json_string));
    esp_http_client_set_header(client, "Content-Type", "application/json");

    esp_err_t err = esp_http_client_perform(client);
    if (err == ESP_OK)
    {

        ESP_LOGI(TAG, "HTTP POST Status = %d", esp_http_client_get_status_code(client));
    }
    else
    {
        ESP_LOGE(TAG, "HTTP POST request failed: %s", esp_err_to_name(err));
    }
    esp_http_client_cleanup(client);
    cJSON_free(my_json_string);
}

// handle data addressed to external IP network
static void root_read_task(void *arg)
{
    mdf_err_t ret = MDF_OK;
    char *data = MDF_MALLOC(MWIFI_PAYLOAD_LEN);
    size_t size = MWIFI_PAYLOAD_LEN;
    uint8_t src_addr[MWIFI_ADDR_LEN] = {0x0};
    mwifi_data_type_t data_type = {0};

    MDF_LOGI("Root read task is running");

    for (int i = 0;; ++i)
    {
        if (!mwifi_is_started())
        {
            vTaskDelay(500 / portTICK_RATE_MS);
            continue;
        }

        size = MWIFI_PAYLOAD_LEN;
        memset(data, 0, MWIFI_PAYLOAD_LEN);
        ret = mwifi_root_read(src_addr, &data_type, data, &size, portMAX_DELAY);
        MDF_ERROR_CONTINUE(ret != MDF_OK, "<%s> mwifi_root_read", mdf_err_to_name(ret));

        if (data_type.upgrade)
        { // This mesh package contains upgrade data.
            ret = mupgrade_root_handle(src_addr, data, size);
            MDF_ERROR_CONTINUE(ret != MDF_OK, "<%s> mupgrade_root_handle", mdf_err_to_name(ret));
        }
        else
        {
            MDF_LOGI("Receive [NODE] addr: " MACSTR ", size: %d, data: %s", MAC2STR(src_addr), size, data);
            char macStr[18];
            // int array[6] = {0x00, 0x0d, 0x3f, 0xcd, 0x02, 0x5f};

            snprintf(macStr, sizeof(macStr), "%02x:%02x:%02x:%02x:%02x:%02x",
                     src_addr[0], src_addr[1], src_addr[2], src_addr[3], src_addr[4], src_addr[5]);
            post_rest_function(data, macStr);
        }
    }

    MDF_LOGW("Root is exit");

    MDF_FREE(data);
    vTaskDelete(NULL);
}

// handle data addressed to root node
static void node_read_task(void *arg)
{
    mdf_err_t ret = MDF_OK;
    char *data = MDF_MALLOC(MWIFI_PAYLOAD_LEN);
    size_t size = MWIFI_PAYLOAD_LEN;
    mwifi_data_type_t data_type = {0x0};
    uint8_t src_addr[MWIFI_ADDR_LEN] = {0};

    MDF_LOGI("Node read task is running");

    for (;;)
    {
        if (!mwifi_is_connected())
        {
            vTaskDelay(500 / portTICK_RATE_MS);
            continue;
        }

        size = MWIFI_PAYLOAD_LEN;
        memset(data, 0, MWIFI_PAYLOAD_LEN);
        ret = mwifi_read(src_addr, &data_type, data, &size, portMAX_DELAY);
        MDF_ERROR_CONTINUE(ret != MDF_OK, "<%s> mwifi_root_recv", mdf_err_to_name(ret));

        if (data_type.upgrade)
        { // This mesh package contains upgrade data.
            ret = mupgrade_handle(src_addr, data, size);
            MDF_ERROR_CONTINUE(ret != MDF_OK, "<%s> mupgrade_handle", mdf_err_to_name(ret));
        }
        else
        {
            MDF_LOGI("Receive [ROOT] addr: " MACSTR ", size: %d, data: %s",
                     MAC2STR(src_addr), size, data);

            switch (data_type.custom)
            {
            case CMD_Restart:
                MDF_LOGI("Restarting the node...");
                MDF_LOGW("The device will restart after 1 seconds");
                vTaskDelay(pdMS_TO_TICKS(1000));
                esp_restart();
            }
        }
    }

    MDF_LOGW("Node read task is exit");

    MDF_FREE(data);
    vTaskDelete(NULL);
}

/**
 * @brief Timed printing system information
 */
static void print_system_info_timercb(void *timer)
{
    uint8_t primary = 0;
    wifi_second_chan_t second = 0;
    mesh_addr_t parent_bssid = {0};
    uint8_t sta_mac[MWIFI_ADDR_LEN] = {0};
    wifi_sta_list_t wifi_sta_list = {0x0};

    esp_wifi_get_mac(ESP_IF_WIFI_STA, sta_mac);
    esp_wifi_ap_get_sta_list(&wifi_sta_list);
    esp_wifi_get_channel(&primary, &second);
    esp_mesh_get_parent_bssid(&parent_bssid);

    MDF_LOGI("System information, channel: %d, layer: %d, self mac: " MACSTR ", parent bssid: " MACSTR
             ", parent rssi: %d, node num: %d, free heap: %u",
             primary,
             esp_mesh_get_layer(), MAC2STR(sta_mac), MAC2STR(parent_bssid.addr),
             mwifi_get_parent_rssi(), esp_mesh_get_total_node_num(), esp_get_free_heap_size());

    for (int i = 0; i < wifi_sta_list.num; i++)
    {
        MDF_LOGI("Child mac: " MACSTR, MAC2STR(wifi_sta_list.sta[i].mac));
    }

#ifdef MEMORY_DEBUG

    if (!heap_caps_check_integrity_all(true))
    {
        MDF_LOGE("At least one heap is corrupt");
    }

    mdf_mem_print_heap();
    mdf_mem_print_record();
    mdf_mem_print_task();
#endif /**< MEMORY_DEBUG */
}

static mdf_err_t wifi_init()
{
    mdf_err_t ret = nvs_flash_init();
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();

    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        MDF_ERROR_ASSERT(nvs_flash_erase());
        ret = nvs_flash_init();
    }

    MDF_ERROR_ASSERT(ret);

    MDF_ERROR_ASSERT(esp_netif_init());
    MDF_ERROR_ASSERT(esp_event_loop_create_default());
    MDF_ERROR_ASSERT(esp_netif_create_default_wifi_mesh_netifs(&sta_netif, NULL));
    MDF_ERROR_ASSERT(esp_wifi_init(&cfg));
    MDF_ERROR_ASSERT(esp_wifi_set_storage(WIFI_STORAGE_FLASH));
    MDF_ERROR_ASSERT(esp_wifi_set_mode(WIFI_MODE_STA));
    MDF_ERROR_ASSERT(esp_wifi_set_ps(WIFI_PS_NONE));
    MDF_ERROR_ASSERT(esp_mesh_set_6m_rate(false));
    MDF_ERROR_ASSERT(esp_wifi_start());

    return MDF_OK;
}

/**
 * @brief All module events will be sent to this task in esp-mdf
 *
 * @Note:
 *     1. Do not block or lengthy operations in the callback function.
 *     2. Do not consume a lot of memory in the callback function.
 *        The task memory of the callback function is only 4KB.
 */
static mdf_err_t event_loop_cb(mdf_event_loop_t event, void *ctx)
{
    MDF_LOGI("event_loop_cb, event: %d", event);

    switch (event)
    {
    case MDF_EVENT_MWIFI_STARTED:
        MDF_LOGI("MESH is started");
        break;

    case MDF_EVENT_MWIFI_PARENT_CONNECTED:
        MDF_LOGI("Parent is connected on station interface");
        esp_netif_dhcpc_start(sta_netif);
        break;

    case MDF_EVENT_MWIFI_PARENT_DISCONNECTED:
        MDF_LOGI("Parent is disconnected on station interface");
        break;

    case MDF_EVENT_MWIFI_ROOT_GOT_IP:
    {
        MDF_LOGI("Root obtains the IP address");
        break;
    }

    default:
        break;
    }

    otaHandleEvent(event, ctx);

    return MDF_OK;
}

void app_main()
{
    mwifi_init_config_t cfg = MWIFI_INIT_CONFIG_DEFAULT();
    mwifi_config_t config = {
        .channel = CONFIG_MESH_CHANNEL,
        .mesh_id = CONFIG_MESH_ID,
        .mesh_type = CONFIG_DEVICE_TYPE,
        .router_ssid = ROUTER_SSID,
        .router_password = ROUTER_PASS};

    /**
     * @brief Set the log level for serial port printing.
     */
    esp_log_level_set("*", ESP_LOG_INFO);
    esp_log_level_set(TAG, ESP_LOG_DEBUG);
    esp_log_level_set("mupgrade_root", ESP_LOG_DEBUG);
    esp_log_level_set("mupgrade_node", ESP_LOG_DEBUG);

    /**
     * @brief Initialize wifi mesh.
     */
    MDF_ERROR_ASSERT(mdf_event_loop_init(event_loop_cb));
    MDF_ERROR_ASSERT(wifi_init());
    MDF_ERROR_ASSERT(mwifi_init(&cfg));
    MDF_ERROR_ASSERT(mwifi_set_config(&config));
    MDF_ERROR_ASSERT(mwifi_start());

    /**
     * @brief Data transfer between wifi mesh devices
     */
    xTaskCreate(root_read_task, "root_read_task", 4 * 1024,
                NULL, CONFIG_MDF_TASK_DEFAULT_PRIOTY, NULL);

    xTaskCreate(node_read_task, "node_read_task", 4 * 1024,
                NULL, CONFIG_MDF_TASK_DEFAULT_PRIOTY, NULL);

    TimerHandle_t timer = xTimerCreate("print_system_info", 10000 / portTICK_RATE_MS,
                                       true, NULL, print_system_info_timercb);
    xTimerStart(timer, 0);

    udpInit();
}
