#ifndef MESSAGE_PARSER_H
#define MESSAGE_PARSER_H

#include <stdbool.h>
#include <stddef.h>
#include "control_types.h"
#include "esp_websocket_client.h" // WebSocket istemci başlığını ekleyin

/**
 * @brief WebSocket mesajını ayrıştırır
 *
 * @param message Ayrıştırılacak mesaj
 * @param length Mesaj uzunluğu
 * @param client WebSocket istemci handle
 * @return bool İşlem başarılı ise true, değilse false
 */
bool parse_websocket_message(const char* message, size_t length, esp_websocket_client_handle_t client);

/**
 * @brief Ayrıştırılmış cihaz kontrolü işlemini gerçekleştirir
 *
 * @param device_id Cihaz ID'si
 * @param control_type Kontrol tipi
 * @param value Kontrol değeri
 * @param client WebSocket istemci handle
 */
void process_device_control(int device_id, control_type_t control_type, const char* value, esp_websocket_client_handle_t client);

#endif // MESSAGE_PARSER_H