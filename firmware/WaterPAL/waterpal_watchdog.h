#ifndef WATERPAL_WATCHDOG_H
#define WATERPAL_WATCHDOG_H
#include <Arduino.h>
#include <time.h>
#include <sys/time.h>
#include <esp_sleep.h> // ESP32 Deep Sleep
#include <esp_task_wdt.h> // ESP32 Task Watchdog Timer

#define WATERPAL_WDT_TIMEOUT_SEC  (60) // Max is 60s for task WDT

void watchdog_enable() {
    esp_task_wdt_deinit(); // Deinitialize if already running

    // If the TWDT was not initialized automatically on startup, manually intialize it now
    esp_task_wdt_config_t twdt_config = {
        .timeout_ms = WATERPAL_WDT_TIMEOUT_SEC * 1000,
        .idle_core_mask = (1 << CONFIG_FREERTOS_NUMBER_OF_CORES) - 1,    // Bitmask of all cores
        .trigger_panic = true,
    };
    ESP_ERROR_CHECK(esp_task_wdt_init(&twdt_config));
    esp_task_wdt_add(NULL); // Add current thread to WDT
    Serial.println("Watchdog enabled");
}

void watchdog_disable() {
    esp_task_wdt_delete(NULL); // Remove current thread from WDT
    esp_task_wdt_deinit();
    Serial.println("Watchdog disabled");
}

void watchdog_pet() {
    // Pet the watchdog to prevent it from triggering
    esp_task_wdt_reset();
    Serial.println("Watchdog petted");
}

#endif