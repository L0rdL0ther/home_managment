/**
* @file wifi_control.h
 * @brief ESP32 WiFi kontrol modülü
 */
#ifndef WIFI_CONTROL_H
#define WIFI_CONTROL_H

#include <esp_err.h>
#include <esp_netif.h>
#include <stdbool.h>

/**
 * @brief WiFi bağlantı durumları
 */
typedef enum {
    WIFI_STATUS_DISCONNECTED = 0, ///< WiFi bağlantısı yok
    WIFI_STATUS_CONNECTING,       ///< WiFi bağlanıyor
    WIFI_STATUS_CONNECTED,        ///< WiFi bağlantısı aktif
    WIFI_STATUS_FAILED            ///< WiFi bağlantısı başarısız oldu
} wifi_connection_status_t;

/**
 * @brief WiFi yapılandırma parametreleri
 */
typedef struct {
    const char *ssid;             ///< WiFi SSID
    const char *password;         ///< WiFi Şifresi
    int max_retry;                ///< Maksimum yeniden bağlantı denemesi
    bool auto_reconnect;          ///< Otomatik yeniden bağlantı
    int retry_interval_ms;        ///< Yeniden bağlantı aralığı (ms)
} wifi_config_params_t;

/**
 * @brief WiFi modülünü başlatır ve bağlantı kurar
 */
esp_err_t wifi_control_init(const wifi_config_params_t *config);

/**
 * @brief Mevcut bir WiFi bağlantısını keser
 */
esp_err_t wifi_control_disconnect(void);

/**
 * @brief WiFi modülünü durdurur ve kaynakları serbest bırakır
 */
esp_err_t wifi_control_deinit(void);

/**
 * @brief Mevcut WiFi bağlantı durumunu sorgular
 */
wifi_connection_status_t wifi_control_get_status(void);

/**
 * @brief Bağlı olunan IP adresini döndürür
 */
esp_err_t wifi_control_get_ip_info(esp_netif_ip_info_t *ip_info);

/**
 * @brief Bağlantı yeniden kurulumu için bir deneme başlatır
 */
esp_err_t wifi_control_reconnect(void);

#endif // WIFI_CONTROL_H