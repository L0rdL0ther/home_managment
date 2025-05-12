//
// Created by yusuf on 26.03.2025.
// 

#include "smart_home.h"
#include "control_types.h"
#include <esp_websocket_client.h>
#include <stdbool.h>
#include <driver/gpio.h>
#include "esp_log.h"
#include "string.h"
#include "stdlib.h"
#include <esp_err.h>
#include <esp_http_server.h>

static const char *TAG = "SMART_HOME";

// WebSocket client and associated data
typedef struct {
    esp_websocket_client_handle_t client;
    message_callback_t callback;
    void *user_context;
    char *auth_token;
    bool is_authenticated;
    bool is_connected;
} smart_home_context_t;

static smart_home_context_t s_context = {0};

// Function to convert control type to string
static control_type_t string_to_control_type(const char *type_str) {
    static const struct {
        const char *str;
        control_type_t type;
    } type_map[] = {
        {"SWITCH", CONTROL_TYPE_SWITCH},
        {"SLIDER", CONTROL_TYPE_SLIDER},
        {"RGB_PICKER", CONTROL_TYPE_RGB_PICKER},
        {"BUTTON_GROUP", CONTROL_TYPE_BUTTON_GROUP},
        {"NUMERIC_INPUT", CONTROL_TYPE_NUMERIC_INPUT},
        {"TEXT_DISPLAY", CONTROL_TYPE_TEXT_DISPLAY},
        {"DROPDOWN", CONTROL_TYPE_DROPDOWN},
        {"SCHEDULE", CONTROL_TYPE_SCHEDULE},
    };

    for (int i = 0; i < sizeof(type_map) / sizeof(type_map[0]); i++) {
        if (strcmp(type_str, type_map[i].str) == 0) {
            return type_map[i].type;
        }
    }
    return CONTROL_TYPE_UNKNOWN;
}

// Log error code if non-zero
static void log_error_if_nonzero(const char *message, int error_code) {
    if (error_code != 0) {
        ESP_LOGE(TAG, "Error %s: 0x%x", message, error_code);
    }
}

// Parse WebSocket messages
static bool parse_websocket_message(const char *message, size_t length) {
    if (message == NULL || length == 0) {
        return false;
    }

    // Check for successful connection message
    if (strncmp(message, "Successfully connected", length) == 0) {
        ESP_LOGI(TAG, "Connection successfully authenticated!");
        s_context.is_authenticated = true;
        return true;
    }

    // Check for "datasend:" prefix
    const char *prefix = "datasend:";
    size_t prefix_len = strlen(prefix);
    if (strncmp(message, prefix, prefix_len) != 0) {
        return false;
    }

    // Create a safe copy of the message (for strtok_r usage)
    char *msg_copy = malloc(length + 1);
    if (!msg_copy) {
        ESP_LOGE(TAG, "Memory allocation error");
        return false;
    }

    memcpy(msg_copy, message, length);
    msg_copy[length] = '\0';

    bool success = false;
    char *token;
    char *rest = msg_copy;

    // Skip "datasend:" part
    token = strtok_r(rest, ":", &rest);
    if (!token || strcmp(token, "datasend") != 0) {
        goto cleanup;
    }

    // Get device ID
    token = strtok_r(NULL, ":", &rest);
    if (!token) {
        goto cleanup;
    }
    int device_id = atoi(token);

    // Get control type
    token = strtok_r(NULL, ":", &rest);
    if (!token) {
        goto cleanup;
    }
    control_type_t control_type = string_to_control_type(token);

    // Get value
    token = strtok_r(NULL, ":", &rest);
    if (!token) {
        goto cleanup;
    }

    // Call callback function
    if (s_context.callback) {
        s_context.callback(device_id, control_type, token, s_context.client, s_context.user_context);
        success = true;
    }

cleanup:
    free(msg_copy);
    return success;
}

// WebSocket event handler
static void websocket_event_handler(void *handler_args, esp_event_base_t base,
                                    int32_t event_id, void *event_data) {
    esp_websocket_event_data_t *data = (esp_websocket_event_data_t *)event_data;

    switch (event_id) {
        case WEBSOCKET_EVENT_CONNECTED:
            ESP_LOGI(TAG, "WebSocket connection established");
            s_context.is_connected = true;

            // Send token
            if (s_context.auth_token && strlen(s_context.auth_token) > 0) {
                vTaskDelay(pdMS_TO_TICKS(1000)); // Short delay for connection stability

                esp_err_t err = esp_websocket_client_send_bin(
                    s_context.client,
                    s_context.auth_token,
                    strlen(s_context.auth_token),
                    portMAX_DELAY
                );

                if (err != ESP_OK) {
                    ESP_LOGE(TAG, "Token sending error: %s", esp_err_to_name(err));
                } else {
                    ESP_LOGI(TAG, "Authentication token sent, waiting for validation...");
                }
            }
            break;

        case WEBSOCKET_EVENT_DISCONNECTED:
            ESP_LOGI(TAG, "WebSocket connection lost");
            s_context.is_connected = false;
            s_context.is_authenticated = false;

            log_error_if_nonzero("HTTP status code", data->error_handle.esp_ws_handshake_status_code);

            if (data->error_handle.error_type == WEBSOCKET_ERROR_TYPE_TCP_TRANSPORT) {
                log_error_if_nonzero("ESP-TLS error", data->error_handle.esp_tls_last_esp_err);
                log_error_if_nonzero("TLS stack error", data->error_handle.esp_tls_stack_err);
                log_error_if_nonzero("Socket error", data->error_handle.esp_transport_sock_errno);
            }
            break;

        case WEBSOCKET_EVENT_DATA:
            if (data && data->data_ptr && data->data_len > 0) {
                ESP_LOGI(TAG, "Message received from server: %.*s", data->data_len, (char *)data->data_ptr);

                if (parse_websocket_message((char *)data->data_ptr, data->data_len)) {
                    ESP_LOGI(TAG, "Message processed successfully");
                } else {
                    ESP_LOGW(TAG, "Message could not be processed or unsupported format");
                }
            }
            break;

        case WEBSOCKET_EVENT_ERROR:
            ESP_LOGE(TAG, "WebSocket error");
            log_error_if_nonzero("HTTP status code", data->error_handle.esp_ws_handshake_status_code);

            if (data->error_handle.error_type == WEBSOCKET_ERROR_TYPE_TCP_TRANSPORT) {
                log_error_if_nonzero("ESP-TLS error", data->error_handle.esp_tls_last_esp_err);
                log_error_if_nonzero("TLS stack error", data->error_handle.esp_tls_stack_err);
                log_error_if_nonzero("Socket error", data->error_handle.esp_transport_sock_errno);
            }
            break;

        default:
            break;
    }
}

// Initialize the smart home system
esp_err_t smart_home_init(const smart_home_config_t *config,
                          message_callback_t callback,
                          void *user_context) {
    if (!config || !config->websocket_uri) {
        return ESP_ERR_INVALID_ARG;
    }

    // Check if already initialized
    if (s_context.client) {
        ESP_LOGW(TAG, "Smart home system already initialized");
        return ESP_ERR_INVALID_STATE;
    }

    // Clear context
    memset(&s_context, 0, sizeof(s_context));

    // Create token copy
    if (config->auth_token && strlen(config->auth_token) > 0) {
        s_context.auth_token = strdup(config->auth_token);
        if (!s_context.auth_token) {
            ESP_LOGE(TAG, "Memory allocation failed for token");
            return ESP_ERR_NO_MEM;
        }
    }

    // WebSocket configuration
    esp_websocket_client_config_t ws_cfg = {
        .uri = config->websocket_uri,
        .reconnect_timeout_ms = config->reconnect_timeout_ms > 0 ?
                               config->reconnect_timeout_ms : 10000,
        .disable_auto_reconnect = !config->auto_reconnect,
    };

    s_context.client = esp_websocket_client_init(&ws_cfg);
    if (!s_context.client) {
        ESP_LOGE(TAG, "WebSocket client initialization failed");
        free(s_context.auth_token);
        s_context.auth_token = NULL;
        return ESP_FAIL;
    }

    // Register callback and user context
    s_context.callback = callback;
    s_context.user_context = user_context;

    // Register event listeners
    esp_err_t err = esp_websocket_register_events(
        s_context.client,
        WEBSOCKET_EVENT_ANY,
        websocket_event_handler,
        NULL
    );

    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to register WebSocket events: %s", esp_err_to_name(err));
        esp_websocket_client_destroy(s_context.client);
        free(s_context.auth_token);
        s_context.client = NULL;
        s_context.auth_token = NULL;
        return err;
    }

    // Start WebSocket client
    err = esp_websocket_client_start(s_context.client);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start WebSocket client: %s", esp_err_to_name(err));
        esp_websocket_client_destroy(s_context.client);
        free(s_context.auth_token);
        s_context.client = NULL;
        s_context.auth_token = NULL;
        return err;
    }

    ESP_LOGI(TAG, "Smart home system initialized, WebSocket connection establishing...");
    return ESP_OK;
}

// General helper function to send messages
static esp_err_t send_message(const char *message) {
    if (!s_context.client || !s_context.is_connected) {
        return ESP_ERR_INVALID_STATE;
    }

    esp_err_t err = esp_websocket_client_send_bin(
        s_context.client,
        message,
        strlen(message),
        portMAX_DELAY
    );

    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Message sending error: %s", esp_err_to_name(err));
    } else {
        ESP_LOGI(TAG, "Message sent: %s", message);
    }

    return err;
}



// Bind device to server
esp_err_t smart_home_bind_device(int device_id, const char *bind_value) {
    if (!bind_value) {
        return ESP_ERR_INVALID_ARG;
    }

    char bind_message[64];
    snprintf(bind_message, sizeof(bind_message), "bind:%d:%s", device_id, bind_value);
    return send_message(bind_message);
}

// Deinitialize smart home system
esp_err_t smart_home_deinit(void) {
    if (!s_context.client) {
        return ESP_ERR_INVALID_STATE;
    }

    esp_err_t err = esp_websocket_client_stop(s_context.client);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "WebSocket client stop failed: %s", esp_err_to_name(err));
    }

    esp_websocket_client_destroy(s_context.client);
    free(s_context.auth_token);

    s_context.client = NULL;
    s_context.auth_token = NULL;
    s_context.callback = NULL;
    s_context.user_context = NULL;
    s_context.is_connected = false;
    s_context.is_authenticated = false;

    ESP_LOGI(TAG, "Smart home system shut down");
    return ESP_OK;
}
