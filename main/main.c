#include <esp_err.h>
#include <esp_event.h>
#include <esp_http_server.h>
#include <esp_log.h>
#include <esp_netif.h>
#include <esp_netif_types.h>
#include <esp_wifi.h>
#include <esp_wifi_default.h>
#include <string.h>
#include <driver/gpio.h>
#include <freertos/event_groups.h>
#include "nvs_flash.h"
#include "esp_websocket_client.h"
#include "message_parser.h"  // Yeni ekledik

#define NETWORK_SSID "SUPERONLINE_Wi-Fi_7279"
#define NETWORK_PASSWORD "PYKydx3PAuTe"
#define MAXIMUM_RETRY  5
#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT      BIT1
#define TOKEN "auth:IDYoS7QMIAURA1aGos"
#define WEBSOCKET_URI "ws://192.168.1.5:8080/ws/esp32"

static const char *TAG = "home_managment";
static EventGroupHandle_t s_wifi_event_group;
static int s_retry_num = 0;
static esp_netif_t *sta_netif = NULL;
esp_websocket_client_handle_t client = NULL;

void event_handler(void *arg, esp_event_base_t event_base,
                   int32_t event_id, void *event_data) {
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        ESP_LOGI(TAG, "WiFi başlatıldı, bağlanmaya çalışılıyor...");
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        if (s_retry_num < MAXIMUM_RETRY) {
            esp_wifi_connect();
            s_retry_num++;
            ESP_LOGI(TAG, "Yeniden WiFi'ye bağlanılıyor... Deneme %d/%d",
                     s_retry_num, MAXIMUM_RETRY);
        } else {
            xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
            ESP_LOGI(TAG, "WiFi bağlantısı başarısız oldu");
        }
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *event = (ip_event_got_ip_t *) event_data;
        ESP_LOGI(TAG, "IP adresi alındı: " IPSTR, IP2STR(&event->ip_info.ip));
        s_retry_num = 0;
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
    }
}

void wifi_init_sta(void) {
    esp_err_t ret = nvs_flash_init();

    if (ret == ESP_ERR_NVS_NO_FREE_PAGES) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
    ESP_LOGI(TAG, "WiFi initialize started.");
    s_wifi_event_group = xEventGroupCreate();
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    sta_netif = esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    esp_event_handler_instance_t instance_any_id;
    esp_event_handler_instance_t instance_got_ip;

    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
        ESP_EVENT_ANY_ID,
        &event_handler,
        NULL,
        &instance_any_id));

    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
        IP_EVENT_STA_GOT_IP,
        &event_handler,
        NULL,
        &instance_got_ip));

    wifi_config_t wifi_config = {
        .sta = {
            .threshold.authmode = WIFI_AUTH_WPA2_PSK
        }
    };
    strcpy((char *) wifi_config.sta.ssid, NETWORK_SSID);
    strcpy((char *) wifi_config.sta.password, NETWORK_PASSWORD);

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "WiFi started.");

    EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group,
                                           WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
                                           pdFALSE, // Bitleri temizleme
                                           pdFALSE,
                                           portMAX_DELAY);

    if (bits & WIFI_CONNECTED_BIT) {
        ESP_LOGI(TAG, "WiFi ağına bağlandı: %s", NETWORK_SSID);
    } else if (bits & WIFI_FAIL_BIT) {
        ESP_LOGI(TAG, "WiFi ağına bağlanılamadı: %s", NETWORK_SSID);
    } else {
        ESP_LOGE(TAG, "BEKLENMEYEN DURUM");
    }
}

static void log_error_if_nonzero(const char *message, int error_code) {
    if (error_code != 0) {
        ESP_LOGE(TAG, "Last error %s: 0x%x", message, error_code);
    }
}

static void websocket_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data) {
    esp_websocket_event_data_t *data = (esp_websocket_event_data_t *) event_data;
    switch (event_id) {
        case WEBSOCKET_EVENT_BEGIN:
            ESP_LOGI(TAG, "WEBSOCKET_EVENT_BEGIN");
            break;
        case WEBSOCKET_EVENT_CONNECTED:
            ESP_LOGI(TAG, "WEBSOCKET_EVENT_CONNECTED");

            if (!esp_websocket_client_is_connected(client))
                break;

            vTaskDelay(1000 / portTICK_PERIOD_MS);

            if (client != NULL) {
                // Token'ı binary olarak gönder
                esp_err_t err = esp_websocket_client_send_bin(client,
                                                              (const char *) TOKEN,
                                                              strlen(TOKEN),
                                                              portMAX_DELAY);

                if (err != ESP_OK) {
                    ESP_LOGE(TAG, "Token gönderme hatası: %s", esp_err_to_name(err));
                } else {
                    ESP_LOGI(TAG, "Binary token gönderildi, doğrulama bekleniyor...");
                }
            }


            const char *setValue = "bind:1:1";
            esp_websocket_client_send_bin(client, setValue, strlen(setValue), portMAX_DELAY);

            break;
        case WEBSOCKET_EVENT_DISCONNECTED:
            ESP_LOGI(TAG, "WEBSOCKET_EVENT_DISCONNECTED");
            log_error_if_nonzero("HTTP status code", data->error_handle.esp_ws_handshake_status_code);
            if (data->error_handle.error_type == WEBSOCKET_ERROR_TYPE_TCP_TRANSPORT) {
                log_error_if_nonzero("reported from esp-tls", data->error_handle.esp_tls_last_esp_err);
                log_error_if_nonzero("reported from tls stack", data->error_handle.esp_tls_stack_err);
                log_error_if_nonzero("captured as transport's socket errno",
                                     data->error_handle.esp_transport_sock_errno);
            }
            break;
        case WEBSOCKET_EVENT_DATA:
            if (data != NULL && data->data_ptr && data->data_len > 0) {
                ESP_LOGI(TAG, "Sunucudan mesaj alındı: %.*s", data->data_len, (char *)data->data_ptr);

                // Mesajı parse et - client handle'ı geçir
                if (parse_websocket_message((char *)data->data_ptr, data->data_len, client)) {
                    ESP_LOGI(TAG, "Mesaj başarıyla işlendi");
                } else {
                    ESP_LOGW(TAG, "Mesaj işlenemedi veya desteklenmeyen format");
                }
            }
        break;
        case WEBSOCKET_EVENT_ERROR:
            ESP_LOGI(TAG, "WEBSOCKET_EVENT_ERROR");
            log_error_if_nonzero("HTTP status code", data->error_handle.esp_ws_handshake_status_code);
            if (data->error_handle.error_type == WEBSOCKET_ERROR_TYPE_TCP_TRANSPORT) {
                log_error_if_nonzero("reported from esp-tls", data->error_handle.esp_tls_last_esp_err);
                log_error_if_nonzero("reported from tls stack", data->error_handle.esp_tls_stack_err);
                log_error_if_nonzero("captured as transport's socket errno",
                                     data->error_handle.esp_transport_sock_errno);
            }
            break;
        case WEBSOCKET_EVENT_FINISH:
            ESP_LOGI(TAG, "WEBSOCKET_EVENT_FINISH");
            break;
        default: ;
    }
}

void app_main(void) {
    ESP_LOGI(TAG, "Starting... App.");

    gpio_config_t io_conf = {
        .intr_type = GPIO_INTR_DISABLE,
        .mode = GPIO_MODE_OUTPUT,
        .pin_bit_mask = (1ULL << GPIO_NUM_19),
        .pull_down_en = 0,
        .pull_up_en = 0
    };
    gpio_config(&io_conf);

    wifi_init_sta();
    const esp_websocket_client_config_t ws_cfg = {
        .uri = WEBSOCKET_URI,
    };

    client = esp_websocket_client_init(&ws_cfg);
    if (client == NULL) {
        ESP_LOGE(TAG, "WebSocket başlatılamadı!");
        return;
    }

    esp_websocket_register_events(client, WEBSOCKET_EVENT_ANY, websocket_event_handler, (void *) client);

    esp_err_t start_ret = esp_websocket_client_start(client);
    if (start_ret != ESP_OK) {
        ESP_LOGE(TAG, "WebSocket başlatma hatası: %s", esp_err_to_name(start_ret));
        return;
    }
    esp_netif_ip_info_t ip_info;
    esp_netif_get_ip_info(sta_netif, &ip_info);
    ESP_LOGI(TAG, "Web sunucu çalışıyor! Web tarayıcınızda şu adrese gidin: http://"IPSTR"/health",
             IP2STR(&ip_info.ip));



    while (1) {
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
}