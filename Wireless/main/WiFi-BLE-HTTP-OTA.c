#include <stdio.h>
#include <string.h>
#include <assert.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_log.h"
#include "esp_system.h"
#include "esp_event.h"
#include "nvs_flash.h"
#include "esp_netif.h"
#include "esp_wifi.h"

/* ===== OTA / HTTP ===== */
#include "esp_http_server.h"
#include "esp_ota_ops.h"

/* ===== BLE (NimBLE) ===== */
#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "host/ble_hs.h"
#include "services/gap/ble_svc_gap.h"

static const char *TAG = "ESP32C5";

/* =========================================================
 *                     Wi-Fi SoftAP
 * ========================================================= */
static void wifi_init_softap(void)
{
    esp_netif_t *ap = esp_netif_create_default_wifi_ap();
    assert(ap);

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_ERROR_CHECK(esp_wifi_set_ps(WIFI_PS_NONE));

    wifi_country_t country = {
        .cc = "IN",
        .schan = 1,
        .nchan = 13,
        .policy = WIFI_COUNTRY_POLICY_MANUAL
    };
    ESP_ERROR_CHECK(esp_wifi_set_country(&country));

    wifi_config_t wifi_config = {
        .ap = {
            .ssid = "ESP32C5",
            .password = "231b26458",
            .ssid_len = 0,
            .channel = 1,
            .authmode = WIFI_AUTH_WPA2_PSK,
            .max_connection = 4
        }
    };

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "Wi-Fi SoftAP started (192.168.4.1)");
}

/* =========================================================
 *                     OTA WEB SERVER
 * ========================================================= */
static esp_err_t ota_post_handler(httpd_req_t *req)
{
    esp_ota_handle_t ota_handle;
    const esp_partition_t *update_partition =
        esp_ota_get_next_update_partition(NULL);

    ESP_LOGI(TAG, "Writing to partition %s", update_partition->label);

    ESP_ERROR_CHECK(esp_ota_begin(update_partition, OTA_SIZE_UNKNOWN, &ota_handle));

    char buffer[1024];
    int received;

    while ((received = httpd_req_recv(req, buffer, sizeof(buffer))) > 0) {
        ESP_ERROR_CHECK(esp_ota_write(ota_handle, buffer, received));
    }

    ESP_ERROR_CHECK(esp_ota_end(ota_handle));
    ESP_ERROR_CHECK(esp_ota_set_boot_partition(update_partition));

    httpd_resp_sendstr(req, "OTA update successful. Rebooting...");
    vTaskDelay(pdMS_TO_TICKS(1000));
    esp_restart();

    return ESP_OK;
}

static esp_err_t root_get_handler(httpd_req_t *req)
{
    const char html[] =
        "<html><body>"
        "<h2>ESP32-C5 OTA Update</h2>"
        "<form method='POST' action='/update' enctype='application/octet-stream'>"
        "<input type='file' name='firmware'>"
        "<input type='submit' value='Update'>"
        "</form></body></html>";

    httpd_resp_send(req, html, HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

static void start_ota_server(void)
{
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.stack_size = 8192;

    httpd_handle_t server = NULL;
    ESP_ERROR_CHECK(httpd_start(&server, &config));

    httpd_uri_t root = {
        .uri = "/",
        .method = HTTP_GET,
        .handler = root_get_handler
    };

    httpd_uri_t update = {
        .uri = "/update",
        .method = HTTP_POST,
        .handler = ota_post_handler
    };

    httpd_register_uri_handler(server, &root);
    httpd_register_uri_handler(server, &update);

    ESP_LOGI(TAG, "OTA server running");
}

/* =========================================================
 *                     BLE (STABLE)
 * ========================================================= */
static void ble_host_task(void *param)
{
    nimble_port_run();
    nimble_port_freertos_deinit();
}

static void ble_on_sync(void)
{
    uint8_t addr_type;
    ble_hs_id_infer_auto(0, &addr_type);

    ble_svc_gap_device_name_set("ESP32C5");

    struct ble_gap_adv_params adv = {0};
    adv.conn_mode = BLE_GAP_CONN_MODE_UND;
    adv.disc_mode = BLE_GAP_DISC_MODE_GEN;

    ble_gap_adv_start(addr_type, NULL, BLE_HS_FOREVER, &adv, NULL, NULL);

    ESP_LOGI(TAG, "BLE advertising started");
}

static void ble_init(void)
{
    ESP_ERROR_CHECK(nimble_port_init());
    ble_hs_cfg.sync_cb = ble_on_sync;
    nimble_port_freertos_init(ble_host_task);
}

/* =========================================================
 *                     app_main
 * ========================================================= */
void app_main(void)
{
    ESP_ERROR_CHECK(nvs_flash_init());
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    ESP_LOGI(TAG, "System booted");

    wifi_init_softap();
    start_ota_server();
    ble_init();

    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
