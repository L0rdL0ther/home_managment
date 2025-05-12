#include <stdio.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <esp_log.h>
#include <esp_websocket_client.h>
#include <driver/gpio.h>
#include "wifi_control/wifi_control.h"
#include "smart_home/smart_home.h"
#include "dht11.h"

#define NETWORK_SSID "sanne"
#define NETWORK_PASSWORD "sanne"
#define AUTH_TOKEN "auth:PIgXssg1dhXIGpcJxiri7Py6J5wYfangki"
#define WEBSOCKET_URI "ws://192.168.1.5:8080/ws/esp32"
#define RELAY_GPIO GPIO_NUM_19

static const char *TAG = "home_managment";
static void message_callback(int device_id, control_type_t control_type,
                           const char *value, esp_websocket_client_handle_t client,
                           void *user_context) {
    ESP_LOGI(TAG, "Mesaj alındı: ID=%d, Tip=%d, Değer=%s", device_id, control_type, value);

    switch (control_type) {
        case CONTROL_TYPE_SWITCH:
                if (device_id == 102) {
                    bool relay_state = (strcmp(value, "0") == 0);
                    gpio_set_level(RELAY_GPIO, relay_state);
                    ESP_LOGI(TAG, "Röle (GPIO 19) %s", relay_state ? "AÇILDI" : "KAPATILDI");
                    smart_home_bind_device(device_id, relay_state ? "0" : "1");
                } else {
                    ESP_LOGW(TAG, "Bilinmeyen cihaz ID: %d", device_id);
                }
        break;

        case CONTROL_TYPE_SLIDER:
                ESP_LOGW(TAG, "Slider kontrol tipi henüz uygulanmadı (Cihaz ID: %d)", device_id);
        break;

        case CONTROL_TYPE_RGB_PICKER:
                ESP_LOGW(TAG, "RGB kontrolü henüz uygulanmadı (Cihaz ID: %d)", device_id);
        break;

        default:
            ESP_LOGW(TAG, "Desteklenmeyen kontrol tipi: %d (Cihaz ID: %d)", control_type, device_id);
        break;
    }
}

void app_main(void) {
    ESP_LOGI(TAG, "Starting... App.");
    gpio_config_t io_conf = {
        .intr_type = GPIO_INTR_DISABLE,
        .mode = GPIO_MODE_OUTPUT,
        .pin_bit_mask = (1ULL << RELAY_GPIO),
        .pull_down_en = 0,
        .pull_up_en = 0
    };
    gpio_config(&io_conf);

    gpio_num_t dht_gpio = GPIO_NUM_9;
    ESP_LOGI(TAG, "DHT11 sensörü başlatılıyor (GPIO %d)...", dht_gpio);
    DHT11_init(dht_gpio);

    wifi_config_params_t wifi_config = {
        .ssid = NETWORK_SSID,
        .password = NETWORK_PASSWORD,
        .max_retry = 5,
        .auto_reconnect = true,
        .retry_interval_ms = 5000
    };
    esp_err_t ret = wifi_control_init(&wifi_config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "WiFi başlatılamadı: %s", esp_err_to_name(ret));
        return;
    }
    smart_home_config_t smart_home_config = {
        .websocket_uri = WEBSOCKET_URI,
        .auth_token = AUTH_TOKEN,
        .auto_reconnect = true,
        .reconnect_timeout_ms = 10000
    };
    ret = smart_home_init(&smart_home_config, message_callback, NULL);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Akıllı ev sistemi başlatılamadı: %s", esp_err_to_name(ret));
        return;
    }
    smart_home_bind_device(102, "1");

    ESP_LOGI(TAG, "Sistem başarıyla başlatıldı, komutlar bekleniyor...");

    int retry_count = 0;
    int lastTemperature = 0;
    int lastHumidity = 0;

    while (1) {

        struct dht11_reading dht_data = DHT11_read();

        if (dht_data.status == DHT11_OK) {
            ESP_LOGI(TAG, "DHT11 Verileri - Nem: %d%%, Sıcaklık: %d°C",
                     dht_data.humidity, dht_data.temperature);
            char value_str[8];
            if (dht_data.humidity != lastHumidity) {
                sprintf(value_str, "%d", dht_data.humidity);
                smart_home_bind_device(155, value_str);
                lastHumidity = dht_data.humidity;
            }
            if (dht_data.temperature != lastTemperature) {
                sprintf(value_str, "%d", dht_data.temperature);
                smart_home_bind_device(154, value_str);
                lastTemperature = dht_data.temperature;
            }
            retry_count = 0;
        } else {
            if (dht_data.status == DHT11_CRC_ERROR) {
                ESP_LOGW(TAG, "DHT11 CRC hatası!");
            } else if (dht_data.status == DHT11_TIMEOUT_ERROR) {
                ESP_LOGW(TAG, "DHT11 zaman aşımı hatası!");
            }
            retry_count++;
            if (retry_count > 5) {
                ESP_LOGE(TAG, "Sensör bağlantısını kontrol edin!");
                retry_count = 0;
            }
        }

        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
}