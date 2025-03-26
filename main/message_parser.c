#include "message_parser.h"

#include <esp_websocket_client.h>
#include <stdbool.h>
#include <driver/gpio.h>

#include "esp_log.h"
#include "string.h"
#include "stdlib.h"

static const char *TAG = "home_managment";

// Kontrol tipi string'ini enum değerine dönüştürme
static control_type_t string_to_control_type(const char *type_str) {
    if (strcmp(type_str, "SWITCH") == 0) return CONTROL_TYPE_SWITCH;
    if (strcmp(type_str, "SLIDER") == 0) return CONTROL_TYPE_SLIDER;
    if (strcmp(type_str, "RGB_PICKER") == 0) return CONTROL_TYPE_RGB_PICKER;
    if (strcmp(type_str, "BUTTON_GROUP") == 0) return CONTROL_TYPE_BUTTON_GROUP;
    if (strcmp(type_str, "NUMERIC_INPUT") == 0) return CONTROL_TYPE_NUMERIC_INPUT;
    if (strcmp(type_str, "TEXT_DISPLAY") == 0) return CONTROL_TYPE_TEXT_DISPLAY;
    if (strcmp(type_str, "DROPDOWN") == 0) return CONTROL_TYPE_DROPDOWN;
    if (strcmp(type_str, "SCHEDULE") == 0) return CONTROL_TYPE_SCHEDULE;
    return CONTROL_TYPE_UNKNOWN;
}

// Cihaz kontrol işlemini gerçekleştirme
void process_device_control(int device_id, control_type_t control_type, const char *value,
                            esp_websocket_client_handle_t client) {
    ESP_LOGI(TAG, "Cihaz kontrolü: ID=%d, Tip=%d, Değer=%s", device_id, control_type, value);

    char status_message[64]; // Durum mesajı için buffer

    switch (control_type) {
        case CONTROL_TYPE_SWITCH:
            if (device_id == 102) {
                gpio_num_t pin = GPIO_NUM_19;
                if (strcmp(value, "0") == 0) {
                    ESP_LOGI(TAG, "GPIO 19 açıldı");
                    gpio_set_level(pin, 1);
                    int gpio_state = 0;
                    snprintf(status_message, sizeof(status_message), "bind:%d:%d", device_id, gpio_state);
                    esp_websocket_client_send_bin(client, status_message, strlen(status_message), portMAX_DELAY);
                } else if (strcmp(value, "1") == 0) {
                    ESP_LOGI(TAG, "GPIO 19 kapatıldı");
                    gpio_set_level(pin, 0);
                    int gpio_state = 1;
                    snprintf(status_message, sizeof(status_message), "bind:%d:%d", device_id, gpio_state);
                    esp_websocket_client_send_bin(client, status_message, strlen(status_message), portMAX_DELAY);
                }
            }
            break;

        default:
            ESP_LOGW(TAG, "Bu kontrol tipi henüz uygulanmadı: %d", control_type);
            break;
    }
}

// WebSocket mesajını ayrıştırma
bool parse_websocket_message(const char *message, size_t length, esp_websocket_client_handle_t client) {
    if (message == NULL || length == 0) {
        return false;
    }

    // "Successfully connected" mesajını kontrol et
    if (strncmp(message, "Successfully connected", length) == 0) {
        ESP_LOGI(TAG, "Bağlantı başarıyla doğrulandı!");
        return true;
    }

    // "datasend:" prefix kontrolü
    const char *prefix = "datasend:";
    size_t prefix_len = strlen(prefix);
    if (strncmp(message, prefix, prefix_len) != 0) {
        return false;
    }

    // Mesajı kopyala (strtok_r için güvenli bir kopya)
    char *msg_copy = malloc(length + 1);
    if (!msg_copy) {
        ESP_LOGE(TAG, "Bellek ayırma hatası");
        return false;
    }

    memcpy(msg_copy, message, length);
    msg_copy[length] = '\0';

    // Mesajı parçalara ayır
    char *token;
    char *rest = msg_copy;

    // "datasend:" kısmını geç
    token = strtok_r(rest, ":", &rest);
    if (!token || strcmp(token, "datasend") != 0) {
        free(msg_copy);
        return false;
    }

    // Cihaz ID'sini al
    token = strtok_r(NULL, ":", &rest);
    if (!token) {
        free(msg_copy);
        return false;
    }
    int device_id = atoi(token);

    // Kontrol tipini al
    token = strtok_r(NULL, ":", &rest);
    if (!token) {
        free(msg_copy);
        return false;
    }
    control_type_t control_type = string_to_control_type(token);

    // Değeri al
    token = strtok_r(NULL, ":", &rest);
    if (!token) {
        free(msg_copy);
        return false;
    }

    // Cihazı kontrol et - client parametresini de geçir
    process_device_control(device_id, control_type, token, client);

    free(msg_copy);
    return true;
}
