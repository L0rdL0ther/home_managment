#include <esp_err.h>
#include <esp_log.h>
#include "nvs_flash.h"

#define NETWORK_SSID "SUPERONLINE_Wi-Fi_7279"
#define NETWORK_PASSWORD "PYKydx3PAuTe"
#define MAXIMUM_RETRY  5

static const char* tag = "home_managment";

void wifi_init_sta(void) {
    esp_err_t ret = nvs_flash_init();

    if (ret == ESP_ERR_NVS_NO_FREE_PAGES) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    ESP_LOGI
}

void app_main(void) {
}
