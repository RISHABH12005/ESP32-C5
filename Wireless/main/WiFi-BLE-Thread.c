#include <stdio.h>
#include <assert.h>

/* FreeRTOS */
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

/* ESP system */
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_netif.h"
#include "esp_event.h"

/* Wi-Fi */
#include "esp_wifi.h"

/* BLE (NimBLE) */
#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "host/ble_hs.h"
#include "services/gap/ble_svc_gap.h"

/* Thread (OpenThread) */
#include "esp_openthread.h"
#include "esp_openthread_types.h"
#include "esp_openthread_lock.h"

static const char *TAG = "ESP32C5";

/* =====================================================
 *                   Wi-Fi SoftAP
 * ===================================================== */
static void wifi_init_softap(void)
{
    esp_netif_create_default_wifi_ap();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_ERROR_CHECK(esp_wifi_set_ps(WIFI_PS_NONE));

    wifi_config_t ap_cfg = {
        .ap = {
            .ssid = "ESP32C5",
            .password = "231b26458",
            .channel = 1,
            .max_connection = 4,
            .authmode = WIFI_AUTH_WPA2_PSK,
        }
    };

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &ap_cfg));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "Wi-Fi SoftAP started: ESP32C5");
}

/* =====================================================
 *                   BLE (NimBLE)
 * ===================================================== */
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

    struct ble_gap_adv_params adv = {
        .conn_mode = BLE_GAP_CONN_MODE_UND,
        .disc_mode = BLE_GAP_DISC_MODE_GEN,
    };

    ble_gap_adv_start(
        addr_type,
        NULL,
        BLE_HS_FOREVER,
        &adv,
        NULL,
        NULL
    );

    ESP_LOGI(TAG, "BLE advertising started");
}

static void ble_init(void)
{
    ESP_ERROR_CHECK(nimble_port_init());
    ble_hs_cfg.sync_cb = ble_on_sync;
    nimble_port_freertos_init(ble_host_task);
}

/* =====================================================
 *                   Thread (OpenThread)
 * ===================================================== */
static void thread_init(void)
{
    esp_openthread_platform_config_t cfg = {
        .radio_config = {
            .radio_mode = RADIO_MODE_NATIVE,
        },
        .host_config = {
            .host_connection_mode = HOST_CONNECTION_MODE_NONE,
        },
        .port_config = {
            .storage_partition_name = "nvs",
            .netif_queue_size = 10,
            .task_queue_size = 10,
        },
    };

    ESP_ERROR_CHECK(esp_openthread_init(&cfg));
    ESP_LOGI(TAG, "Thread initialized (OpenThread FTD)");
}

/* =====================================================
 *                   app_main
 * ===================================================== */
void app_main(void)
{
    ESP_ERROR_CHECK(nvs_flash_init());
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    ESP_LOGI(TAG, "System start");

    wifi_init_softap();   // Visible in normal Wi-Fi list
    ble_init();           // Visible in BLE scanner apps
    thread_init();        // IEEE 802.15.4 / Thread

    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
