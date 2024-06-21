/* UART asynchronous example, that uses separate RX and TX tasks

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_log.h"
#include "driver/uart.h"
#include "string.h"
#include "driver/gpio.h"
#include "esp_http_client.h"
#include "esp_crt_bundle.h"
#include "nvs_flash.h"

#include "esp_xmodem.h"
#include "esp_xmodem_transport.h"

#include "wifi_connect.h"

#define XMODEM_DATA_LEN 128 /* Normal Xmodem data length */

// static const int RX_BUF_SIZE = 1024;

#define TXD_PIN (GPIO_NUM_2)
#define RXD_PIN (GPIO_NUM_4)
#define OTA_BUF_SIZE 1024
char *TAG = "uart_my";
QueueHandle_t uart0_queue;

static const char *xmodem_url = "yourself URL"; // TODO modifyurl,support https
static char upgrade_data_buf[OTA_BUF_SIZE + 1];
int sendData(const char *data)
{
    const int len = strlen(data);
    const int txBytes = uart_write_bytes(UART_NUM_1, data, len);
    ESP_LOGI(TAG, "Wrote %d bytes", txBytes);
    return txBytes;
}
static void rx_task(void *arg)
{
    static const char *RX_TASK_TAG = "RX_TASK";
    esp_log_level_set(RX_TASK_TAG, ESP_LOG_INFO);
    uint8_t *data = (uint8_t *)malloc(1024 + 1);
    bool start_upload = false;
    while (1)
    {
        const int rxBytes = uart_read_bytes(UART_NUM_1, data, 1024, 20 / portTICK_PERIOD_MS);
        ESP_LOGI("rx_task", "uart_read_bytes %d", rxBytes);
        ESP_LOG_BUFFER_HEXDUMP(RX_TASK_TAG, data, rxBytes, ESP_LOG_INFO);

        vTaskDelay(600 / portTICK_PERIOD_MS);
    }
    free(data);
}
void init(void)
{
    const uart_config_t uart_config = {
        .baud_rate = 115200,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
        .rx_flow_ctrl_thresh = 122,
    };
    // We won't use a buffer for sending data.
    uart_driver_install(UART_NUM_1, 1024 * 2, 1024 * 2, 10, &uart0_queue, 0);
    uart_param_config(UART_NUM_1, &uart_config);
    uart_set_pin(UART_NUM_1, TXD_PIN, RXD_PIN, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);

    uart_pattern_queue_reset(UART_NUM_1, 10);

    xTaskCreate(rx_task, "uart_task", 2048, (void *)UART_NUM_1, 12, NULL);
}
// void app_main(void)
// {
//     init();
//     // xTaskCreate(rx_task, "rx_task", 1024 * 2, NULL, configMAX_PRIORITIES, NULL);

//     // vTaskDelay(2000 / portTICK_PERIOD_MS);
//     // sendData("TX_TASK_TAG", "otaupdate\r\n");

//     // vTaskDelay(3000 / portTICK_PERIOD_MS);
//     // sendData("TX_TASK_TAG", "1");
//     // uart_flush(UART_NUM_1);
//     // uint8_t *data = (uint8_t *)malloc(1024);
//     while (1)
//     {
//         // int len = uart_read_bytes(UART_NUM_1, data, 1024, 100 / portTICK_PERIOD_MS);
//         // if (len > 0)
//         // {
//         //     data[len] = 0;
//         //     ESP_LOGI("app_main", "Read %d bytes: '%s'", len, data);
//         // }
//         vTaskDelay(600 / portTICK_PERIOD_MS);
//     }
//     // xTaskCreate(tx_task, "uart_tx_task", 1024*2, NULL, configMAX_PRIORITIES-1, NULL);
// }

static void http_client_task(void *pvParameters)
{
    esp_xmodem_handle_t xmodem_sender = (esp_xmodem_handle_t)pvParameters;
    esp_err_t err;
    esp_http_client_config_t config = {
        .url = xmodem_url,
        .skip_cert_common_name_check = true,
        .crt_bundle_attach = esp_crt_bundle_attach,
    };

    esp_http_client_handle_t http_client = esp_http_client_init(&config);
    if (http_client == NULL)
    {
        ESP_LOGE(TAG, "Failed to initialise HTTP connection");
        goto err;
    }
    err = esp_http_client_open(http_client, 0);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to open HTTP connection: %s", esp_err_to_name(err));
        esp_http_client_cleanup(http_client);
        goto err;
    }
    int content_length = esp_http_client_fetch_headers(http_client);

    if (content_length <= 0)
    {
        ESP_LOGE(TAG, "No content found in the image");
        goto FAIL;
    }

    int image_data_remaining = content_length;
    int binary_file_len = 0;

#ifdef CONFIG_SUPPORT_FILE
    if (esp_xmodem_sender_send_file_packet(xmodem_sender, FILE_NAME, strlen(FILE_NAME), content_length) != ESP_OK)
    {
        ESP_LOGE(TAG, "Send filename fail");
        goto FAIL;
    }
#endif
    while (image_data_remaining != 0)
    {
        ESP_LOGI(TAG, "http read data image_data_remaining:%d", image_data_remaining);

        int data_max_len;
        if (image_data_remaining < OTA_BUF_SIZE)
        {
            data_max_len = image_data_remaining;
        }
        else
        {
            data_max_len = OTA_BUF_SIZE;
        }
        int data_read = esp_http_client_read(http_client, upgrade_data_buf, data_max_len);
        if (data_read == 0)
        {
            if (errno == ECONNRESET || errno == ENOTCONN)
            {
                ESP_LOGE(TAG, "Connection closed, errno = %d", errno);
                break;
            }
            if (content_length == binary_file_len)
            {
                ESP_LOGI(TAG, "Connection closed,all data received");
                break;
            }
        }
        else if (data_read < 0)
        {
            ESP_LOGE(TAG, "Error: SSL data read error");
            break;
        }
        else if (data_read > 0)
        {
            ESP_LOGI(TAG, "http read data: has read data:%d", data_read);
            err = esp_xmodem_sender_send(xmodem_sender, (uint8_t *)upgrade_data_buf, data_read);
            if (err != ESP_OK)
            {
                ESP_LOGE(TAG, "Error:esp_xmodem_sender_send failed!");
                goto FAIL;
            }

            ESP_LOGI(TAG, "esp_xmodem_sender_send success, go next");
            image_data_remaining -= data_read;
            binary_file_len += data_read;
            ESP_LOGD(TAG, "Written image length %d", binary_file_len);
        }
    }
    ESP_LOGD(TAG, "Total binary data length writen: %d", binary_file_len);

    if (content_length != binary_file_len)
    {
        ESP_LOGE(TAG, "Error in receiving complete file");
        esp_xmodem_sender_send_cancel(xmodem_sender);
        goto FAIL;
    }
    else
    {
        err = esp_xmodem_sender_send_eot(xmodem_sender);

        // if (err != ESP_OK)
        // {
        //     ESP_LOGE(TAG, "Error:esp_xmodem_sender_send_eot FAIL!");
        //     goto FAIL;
        // }
        ESP_LOGI(TAG, "Send image success");
        // esp_xmodem_transport_close(xmodem_sender);
        // esp_xmodem_clean(xmodem_sender);

        vTaskDelay(1000 / portTICK_PERIOD_MS);
        sendData("2");

        // uint8_t *data = (uint8_t *)malloc(1024);
        // while (1)
        // {
        //     int len = uart_read_bytes(UART_NUM_1, data, 1024, 100 / portTICK_PERIOD_MS);
        //     if (len > 0)
        //     {
        //         data[len] = 0;
        //         ESP_LOGI("app_main", "Read %d bytes: '%s'", len, data);

        //         ESP_LOG_BUFFER_HEXDUMP(TAG, data, len, ESP_LOG_INFO);
        //     }
        //     vTaskDelay(1000 / portTICK_PERIOD_MS);
        // }
    }
FAIL:
    esp_http_client_close(http_client);
    esp_http_client_cleanup(http_client);
err:
    vTaskDelete(NULL);
}
bool has_upload = false;
esp_err_t xmodem_sender_event_handler(esp_xmodem_event_t *evt)
{
    switch (evt->event_id)
    {
    case ESP_XMODEM_EVENT_ERROR:
        ESP_LOGI(TAG, "ESP_XMODEM_EVENT_ERROR, err_code is 0x%x, heap size %ld", esp_xmodem_get_error_code(evt->handle), esp_get_free_heap_size());
        if (esp_xmodem_stop(evt->handle) == ESP_OK)
        {
            esp_xmodem_start(evt->handle);
        }
        else
        {
            ESP_LOGE(TAG, "esp_xmodem_stop fail");
            esp_xmodem_transport_close(evt->handle);
            esp_xmodem_clean(evt->handle);
        }
        break;
    case ESP_XMODEM_EVENT_CONNECTED:
        ESP_LOGI(TAG, "ESP_XMODEM_EVENT_CONNECTED, heap size %ld", esp_get_free_heap_size());

        if (has_upload)
        {
        }
        else
        {
            has_upload = true;
            if (xTaskCreate(&http_client_task, "http_client_task", 8192, evt->handle, 5, NULL) == ESP_FAIL)
            {
                ESP_LOGE(TAG, "http_client_task create fail");
                return ESP_FAIL;
            }
        }

        break;
    case ESP_XMODEM_EVENT_FINISHED:
        ESP_LOGI(TAG, "ESP_XMODEM_EVENT_FINISHED");
        break;
    case ESP_XMODEM_EVENT_ON_SEND_DATA:
        ESP_LOGD(TAG, "ESP_XMODEM_EVENT_ON_SEND_DATA, %ld", esp_get_free_heap_size());
        break;
    case ESP_XMODEM_EVENT_ON_RECEIVE_DATA:
        ESP_LOGD(TAG, "ESP_XMODEM_EVENT_ON_RECEIVE_DATA, %ld", esp_get_free_heap_size());
        break;
    case ESP_XMODEM_EVENT_CONNECTING:
        sendData("1");
        break;
    default:
        break;
    }
    return ESP_OK;
}

void after_net_connected()
{

    esp_xmodem_transport_config_t transport_config = {
        .baund_rate = 115200,
        .uart_num = UART_NUM_1,
        .swap_pin = true,
        .tx_pin = TXD_PIN,
        .rx_pin = RXD_PIN,
    };
    esp_xmodem_transport_handle_t transport_handle = esp_xmodem_transport_init(&transport_config);
    if (!transport_handle)
    {
        ESP_LOGE(TAG, "esp_xmodem_transport_init fail");
        return;
    }

    esp_xmodem_config_t config = {
        .role = ESP_XMODEM_SENDER,
        .event_handler = xmodem_sender_event_handler,
        .support_xmodem_1k = false,
    };
    esp_xmodem_handle_t sender = esp_xmodem_init(&config, transport_handle);
    if (sender)
    {
        esp_xmodem_start(sender);
    }
    else
    {
        ESP_LOGE(TAG, "esp_xmodem_init fail");
    }
}
void wifi_connect_event_handler(void *arg, esp_event_base_t event_base,
                                int32_t event_id, void *event_data)
{
    ESP_LOGI(TAG, "wifi_connect_event_handler");
    switch (event_id)
    {
    case 0:
    {
        after_net_connected();
    }
    break;
    case -1:
        break;
    case -2:
        break;
    }
}
void app_main()
{
    esp_err_t ret = nvs_flash_init();
    
    // TODO modify wifi info
    wifiConnect("wifi_ssid", "wifi_password", wifi_connect_event_handler);

    while (1)
    {
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
}
