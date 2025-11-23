#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_netif.h"
#include "esp_http_server.h"

static const char *TAG = "WEB_SERVER";

// --- НАЛАШТУВАННЯ WI-FI ---
#define WIFI_SSID      "YAK" 
#define WIFI_PASS     "Yak1byuk210477"

/* * 1. Функція-обробник (Handler) для URL "/"
 * Вона викликається, коли хтось заходить на IP адресу ESP32
 */
static esp_err_t root_get_handler(httpd_req_t *req)
{
    // Що ми відправляємо браузеру
    const char* resp_str = "<h1>Hello from ESP32! Wadzik really anarchist, suck his bug smock dick</h1><p>Running on ESP-IDF</p>";
    
    // Відправка відповіді
    httpd_resp_send(req, resp_str, HTTPD_RESP_USE_STRLEN);

    // Повертаємо ESP_OK, щоб сервер знав, що все пройшло добре
    return ESP_OK;
}

/* * 2. Функція запуску веб-сервера
 */
static httpd_handle_t start_webserver(void)
{
    httpd_handle_t server = NULL;
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.lru_purge_enable = true; // Очищати старі з'єднання

    // Запускаємо сервер
    ESP_LOGI(TAG, "Starting server on port: '%d'", config.server_port);
    if (httpd_start(&server, &config) == ESP_OK) {
        
        // Налаштовуємо URI (шлях)
        httpd_uri_t root_uri = {
            .uri       = "/",           // Адреса (наприклад, 192.168.1.50/)
            .method    = HTTP_GET,      // Метод GET (звичайний запит браузера)
            .handler   = root_get_handler, // Яку функцію викликати
            .user_ctx  = NULL
        };
        
        // Реєструємо цей URI
        httpd_register_uri_handler(server, &root_uri);
        return server;
    }

    ESP_LOGI(TAG, "Error starting server!");
    return NULL;
}

/*
 * --- Допоміжний код для Wi-Fi (Boilerplate) ---
 * Не лякайся об'єму, це стандартна ініціалізація
 */
static void wifi_event_handler(void* arg, esp_event_base_t event_base,
                               int32_t event_id, void* event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        esp_wifi_connect();
        ESP_LOGI(TAG, "Retrying to connect to the AP");
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        ESP_LOGI(TAG, "Got IP: " IPSTR, IP2STR(&event->ip_info.ip));
        
        // ВАЖЛИВО: Запускаємо сервер тільки коли отримали IP!
        start_webserver();
    }
}

void wifi_init_sta(void)
{
    esp_netif_init();
    esp_event_loop_create_default();
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    esp_wifi_init(&cfg);

    esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL, NULL);
    esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, NULL, NULL);

    wifi_config_t wifi_config = {
        .sta = {
            .ssid = WIFI_SSID,
            .password = WIFI_PASS,
        },
    };
    esp_wifi_set_mode(WIFI_MODE_STA);
    esp_wifi_set_config(WIFI_IF_STA, &wifi_config);
    esp_wifi_start();
}

void app_main(void)
{
    // Ініціалізація NVS (потрібна для Wi-Fi)
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
      ESP_ERROR_CHECK(nvs_flash_erase());
      ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    ESP_LOGI(TAG, "ESP_WIFI_MODE_STA");
    wifi_init_sta();
}