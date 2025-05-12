//
// Created by yusuf on 26.03.2025.
//

#ifndef SMART_HOME_H
#define SMART_HOME_H

#include <stdbool.h>           // For bool type
#include <esp_err.h>           // For esp_err_t type
#include <esp_websocket_client.h> // For esp_websocket_client_handle_t type
#include "control_types.h"

/**
 * @brief Smart Home system configuration structure
 */
typedef struct {
    const char *websocket_uri;   // WebSocket server address
    const char *auth_token;      // Authentication token
    bool auto_reconnect;         // Automatic reconnection when the connection is lost
    int reconnect_timeout_ms;    // Reconnection wait time
} smart_home_config_t;

/**
 * @brief Callback type called when a message is received from the server
 *
 * @param device_id Device ID
 * @param control_type Control type
 * @param value Control value
 * @param client WebSocket client handle
 * @param user_context User-defined context data
 */
typedef void (*message_callback_t)(int device_id, control_type_t control_type,
                                  const char *value, esp_websocket_client_handle_t client,
                                  void *user_context);

/**
 * @brief Initialize the Smart Home system
 *
 * @param config Configuration settings
 * @param callback Message receiving callback function
 * @param user_context User-defined context data
 * @return esp_err_t Success status
 */
esp_err_t smart_home_init(const smart_home_config_t *config,
                          message_callback_t callback,
                          void *user_context);

/**
 * @brief Send bind command for a specific device
 *
 * @param device_id Device ID
 * @param bind_value Binding value
 * @return esp_err_t Success status
 */
esp_err_t smart_home_bind_device(int device_id, const char *bind_value);

/**
 * @brief Stop and clean up the Smart Home system WebSocket client
 *
 * @return esp_err_t Success status
 */
esp_err_t smart_home_deinit(void);

#endif // SMART_HOME_H
