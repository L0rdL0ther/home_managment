//
// Created by yusuf on 26.03.2025.
//

#ifndef SMART_HOME_H
#define SMART_HOME_H

#include <stdbool.h>           // bool tipi için
#include <esp_err.h>           // esp_err_t için
#include <esp_websocket_client.h> // esp_websocket_client_handle_t için
#include "control_types.h"

/**
 * @brief Akıllı ev sistemi yapılandırma yapısı
 */
typedef struct {
    const char *websocket_uri;   // WebSocket sunucu adresi
    const char *auth_token;      // Kimlik doğrulama token'ı
    bool auto_reconnect;         // Bağlantı kesildiğinde otomatik yeniden bağlantı
    int reconnect_timeout_ms;    // Yeniden bağlantı bekleme süresi
} smart_home_config_t;

/**
 * @brief Sunucudan mesaj alındığında çağrılan callback türü
 *
 * @param device_id Cihaz ID'si
 * @param control_type Kontrol tipi
 * @param value Kontrol değeri
 * @param client WebSocket istemci handle
 * @param user_context Kullanıcı tanımlı bağlam verisi
 */
typedef void (*message_callback_t)(int device_id, control_type_t control_type,
                                  const char *value, esp_websocket_client_handle_t client,
                                  void *user_context);

/**
 * @brief Akıllı ev sistemini başlat
 *
 * @param config Yapılandırma ayarları
 * @param callback Mesaj alma callback fonksiyonu
 * @param user_context Kullanıcı tanımlı bağlam verisi
 * @return esp_err_t Başarı durumu
 */
esp_err_t smart_home_init(const smart_home_config_t *config,
                          message_callback_t callback,
                          void *user_context);

/**
 * @brief Sunucuya durumu bildir
 *
 * @param device_id Cihaz ID'si
 * @param value Durum değeri
 * @return esp_err_t Başarı durumu
 */
esp_err_t smart_home_send_status(int device_id, const char *value);

/**
 * @brief Belirli bir cihaz için bind komutunu gönder
 *
 * @param device_id Cihaz ID'si
 * @param bind_value Bağlama değeri
 * @return esp_err_t Başarı durumu
 */
esp_err_t smart_home_bind_device(int device_id, const char *bind_value);

/**
 * @brief Sensör verilerini sunucuya gönder
 *
 * @param temperature Sıcaklık değeri
 * @param humidity Nem değeri
 * @return esp_err_t Başarı durumu
 */
esp_err_t smart_home_send_sensor_data(int temperature, int humidity);

/**
 * @brief Akıllı ev sistemi WebSocket istemcisini durdur ve temizle
 *
 * @return esp_err_t Başarı durumu
 */
esp_err_t smart_home_deinit(void);

#endif // SMART_HOME_H