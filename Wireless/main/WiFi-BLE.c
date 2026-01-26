#include <stdio.h>
#include <assert.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_log.h"
#include "esp_system.h"
#include "nvs_flash.h"
#include "esp_netif.h"
#include "esp_event.h"
#include "esp_wifi.h"

/* ================= BLE (NimBLE) ================= */
#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "host/ble_hs.h"
#include "host/ble_gap.h"
#include "services/gap/ble_svc_gap.h"

static const char *TAG = "ESP32C5";

/* =========================================================
 *                     Wi-Fi SoftAP
 * ========================================================= */
static void wifi_init_softap(void)
{
    /* Create default AP netif (returns pointer, NOT esp_err_t) */
    esp_netif_t *ap_netif = esp_netif_create_default_wifi_ap();
    assert(ap_netif);

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    /* Disable power save for RF stability */
    ESP_ERROR_CHECK(esp_wifi_set_ps(WIFI_PS_NONE));

    /* Country configuration (India) */
    wifi_country_t country = {
        .cc = "IN",
        .schan = 1,
        .nchan = 13,
        .policy = WIFI_COUNTRY_POLICY_MANUAL
    };
    ESP_ERROR_CHECK(esp_wifi_set_country(&country));

    wifi_config_t ap_cfg = {
        .ap = {
            .ssid = "ESP32C5",
            .password = "231b26458",   /* ≥ 8 chars REQUIRED */
            .channel = 1,
            .max_connection = 4,
            .authmode = WIFI_AUTH_WPA2_PSK
        }
    };

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &ap_cfg));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "Wi-Fi SoftAP started (SSID: ESP32C5)");
}

/* =========================================================
 *                     BLE (NimBLE)
 * ========================================================= */

/* NimBLE host task (REQUIRED) */
static void ble_host_task(void *param)
{
    nimble_port_run();
    nimble_port_freertos_deinit();
}

/* Called automatically when BLE stack is ready */
static void ble_app_on_sync(void)
{
    uint8_t own_addr_type;

    /* CRITICAL: never pass NULL */
    ble_hs_id_infer_auto(0, &own_addr_type);

    ble_svc_gap_device_name_set("ESP32C5");

    struct ble_gap_adv_params adv = {0};
    adv.conn_mode = BLE_GAP_CONN_MODE_UND;
    adv.disc_mode = BLE_GAP_DISC_MODE_GEN;

    int rc = ble_gap_adv_start(
        own_addr_type,
        NULL,
        BLE_HS_FOREVER,
        &adv,
        NULL,
        NULL
    );

    if (rc == 0) {
        ESP_LOGI(TAG, "BLE advertising started (name: ESP32C5)");
    } else {
        ESP_LOGE(TAG, "BLE advertising failed, rc=%d", rc);
    }
}

static void ble_init(void)
{
    ESP_ERROR_CHECK(nimble_port_init());
    ble_hs_cfg.sync_cb = ble_app_on_sync;
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

    ESP_LOGI(TAG, "System initialized");

    wifi_init_softap();   /* Wi-Fi discoverable */
    ble_init();           /* BLE discoverable */

    /* Keep main task alive */
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
