/**
 * @file wifi_control.c
 * @brief ESP32 WiFi kontrol modülü uygulaması
 */
#include "wifi_control.h"
#include <esp_log.h>
#include <esp_wifi.h>
#include <esp_event.h>
#include <nvs_flash.h>
#include <freertos/event_groups.h>
#include <string.h>

static const char *TAG = "WIFI_CONTROL";

// WiFi event bitleri
#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT      BIT1

// Statik değişkenler
static EventGroupHandle_t s_wifi_event_group = NULL;
static esp_netif_t *s_sta_netif = NULL;
static int s_retry_num = 0;
static int s_max_retry = 0;
static wifi_connection_status_t s_wifi_status = WIFI_STATUS_DISCONNECTED;
static bool s_is_initialized = false;
static bool s_auto_reconnect = true;
static esp_event_handler_instance_t s_instance_any_id = NULL;
static esp_event_handler_instance_t s_instance_got_ip = NULL;

// WiFi olay işleyicisi
static void wifi_event_handler(void *arg, esp_event_base_t event_base,
                              int32_t event_id, void *event_data)
{
    if (event_base == WIFI_EVENT) {
        if (event_id == WIFI_EVENT_STA_START) {
            ESP_LOGI(TAG, "WiFi başlatıldı, bağlanmaya çalışılıyor...");
            s_wifi_status = WIFI_STATUS_CONNECTING;
            esp_wifi_connect();
        }
        else if (event_id == WIFI_EVENT_STA_DISCONNECTED) {
            if (s_auto_reconnect && s_retry_num < s_max_retry) {
                esp_wifi_connect();
                s_retry_num++;
                s_wifi_status = WIFI_STATUS_CONNECTING;
                ESP_LOGI(TAG, "Yeniden WiFi'ye bağlanılıyor... Deneme %d/%d",
                        s_retry_num, s_max_retry);
            } else {
                xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
                s_wifi_status = WIFI_STATUS_FAILED;
                ESP_LOGI(TAG, "WiFi bağlantısı başarısız oldu");
            }
        }
    }
    else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
        ESP_LOGI(TAG, "WiFi bağlantısı başarılı. IP adresi: " IPSTR,
                IP2STR(&event->ip_info.ip));
        s_retry_num = 0;
        s_wifi_status = WIFI_STATUS_CONNECTED;
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
    }
}

// NVS flash başlatma fonksiyonu
static esp_err_t init_nvs(void)
{
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    return ret;
}

esp_err_t wifi_control_init(const wifi_config_params_t *config)
{
    if (config == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    if (s_is_initialized) {
        ESP_LOGW(TAG, "WiFi zaten başlatılmış, önce wifi_deinit() çağırın");
        return ESP_ERR_INVALID_STATE;
    }

    // NVS'yi başlat
    esp_err_t ret = init_nvs();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "NVS başlatılamadı: %s", esp_err_to_name(ret));
        return ret;
    }

    // Olay grubunu oluştur
    s_wifi_event_group = xEventGroupCreate();
    if (s_wifi_event_group == NULL) {
        ESP_LOGE(TAG, "WiFi olay grubu oluşturulamadı");
        return ESP_ERR_NO_MEM;
    }

    // TCP/IP yığını ve WiFi istasyonu başlat
    ESP_LOGI(TAG, "WiFi başlatılıyor...");
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    s_sta_netif = esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    // Olay dinleyicilerini kaydet
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                      ESP_EVENT_ANY_ID,
                                                      &wifi_event_handler,
                                                      NULL,
                                                      &s_instance_any_id));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
                                                      IP_EVENT_STA_GOT_IP,
                                                      &wifi_event_handler,
                                                      NULL,
                                                      &s_instance_got_ip));

    // WiFi yapılandırmasını ayarla
    wifi_config_t wifi_config = {
        .sta = {
            .threshold.authmode = WIFI_AUTH_WPA2_PSK,
        },
    };

    // SSID ve şifreyi güvenli bir şekilde kopyala
    if (config->ssid != NULL) {
        strlcpy((char *)wifi_config.sta.ssid, config->ssid, sizeof(wifi_config.sta.ssid));
    }

    if (config->password != NULL) {
        strlcpy((char *)wifi_config.sta.password, config->password, sizeof(wifi_config.sta.password));
    }

    // Konfigurasyon değerlerini kaydet
    s_max_retry = (config->max_retry > 0) ? config->max_retry : 5;
    s_auto_reconnect = config->auto_reconnect;
    s_retry_num = 0;

    // WiFi modunu ayarla ve başlat
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "WiFi başlatıldı, \"%s\" ağına bağlanmaya çalışılıyor", config->ssid);
    s_wifi_status = WIFI_STATUS_CONNECTING;
    s_is_initialized = true;

    // Bağlantının tamamlanmasını bekle
    EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group,
                                         WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
                                         pdFALSE, // Bitleri temizleme
                                         pdFALSE, // Her iki biti beklemiyoruz
                                         portMAX_DELAY);

    // Bağlantı durumunu kontrol et
    if (bits & WIFI_CONNECTED_BIT) {
        ESP_LOGI(TAG, "WiFi ağına bağlandı: %s", config->ssid);
        return ESP_OK;
    } else if (bits & WIFI_FAIL_BIT) {
        ESP_LOGE(TAG, "WiFi ağına bağlanılamadı: %s", config->ssid);
        return ESP_FAIL;
    } else {
        ESP_LOGE(TAG, "Beklenmeyen durum");
        return ESP_ERR_INVALID_STATE;
    }
}

esp_err_t wifi_control_disconnect(void)
{
    if (!s_is_initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    esp_err_t ret = esp_wifi_disconnect();
    if (ret == ESP_OK) {
        s_wifi_status = WIFI_STATUS_DISCONNECTED;
        ESP_LOGI(TAG, "WiFi bağlantısı kesildi");
    } else {
        ESP_LOGE(TAG, "WiFi bağlantısı kesilemedi: %s", esp_err_to_name(ret));
    }

    return ret;
}

esp_err_t wifi_control_deinit(void)
{
    if (!s_is_initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    // Önce bağlantıyı kes
    wifi_control_disconnect();

    // Olay işleyicilerini kaldır
    if (s_instance_any_id != NULL) {
        esp_event_handler_instance_unregister(WIFI_EVENT, ESP_EVENT_ANY_ID, s_instance_any_id);
        s_instance_any_id = NULL;
    }

    if (s_instance_got_ip != NULL) {
        esp_event_handler_instance_unregister(IP_EVENT, IP_EVENT_STA_GOT_IP, s_instance_got_ip);
        s_instance_got_ip = NULL;
    }

    // WiFi'yi durdur
    esp_err_t ret = esp_wifi_stop();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "WiFi durdurulamadı: %s", esp_err_to_name(ret));
        return ret;
    }

    // WiFi kaynakları temizle
    ret = esp_wifi_deinit();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "WiFi kaynakları temizlenemedi: %s", esp_err_to_name(ret));
        return ret;
    }

    // Olay grubunu temizle
    if (s_wifi_event_group != NULL) {
        vEventGroupDelete(s_wifi_event_group);
        s_wifi_event_group = NULL;
    }

    s_sta_netif = NULL;
    s_is_initialized = false;
    s_wifi_status = WIFI_STATUS_DISCONNECTED;

    ESP_LOGI(TAG, "WiFi kaynakları temizlendi");
    return ESP_OK;
}

wifi_connection_status_t wifi_control_get_status(void)
{
    return s_wifi_status;
}

esp_err_t wifi_control_get_ip_info(esp_netif_ip_info_t *ip_info)
{
    if (!s_is_initialized || s_sta_netif == NULL || ip_info == NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    return esp_netif_get_ip_info(s_sta_netif, ip_info);
}

esp_err_t wifi_control_reconnect(void)
{
    if (!s_is_initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    // Bağlantı zaten varsa, önce mevcut bağlantıyı kes
    if (s_wifi_status == WIFI_STATUS_CONNECTED) {
        wifi_control_disconnect();
    }

    // Bağlantı parametrelerini sıfırla
    s_retry_num = 0;
    s_wifi_status = WIFI_STATUS_CONNECTING;

    // Yeniden bağlan
    esp_err_t ret = esp_wifi_connect();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Yeniden bağlantı başlatılamadı: %s", esp_err_to_name(ret));
        s_wifi_status = WIFI_STATUS_FAILED;
    } else {
        ESP_LOGI(TAG, "Yeniden bağlantı başlatıldı");
    }

    return ret;
}