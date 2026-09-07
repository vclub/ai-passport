#include "buddy.h"
#include "bsp_button.h"
#include "bsp_display.h"
#include "bsp_pins.h"
#include "bsp_pm.h"

#include "esp_log.h"
#include "nvs_flash.h"

static const char *TAG = "main";

static void on_key(bsp_btn_t btn, bsp_btn_ev_t ev, void *user)
{
    (void)user;
    buddy_on_key(btn, ev);
}

void app_main(void)
{
    ESP_LOGI(TAG, "FoloToy AI Passport Buddy");

    esp_err_t e = nvs_flash_init();
    if (e == ESP_ERR_NVS_NO_FREE_PAGES || e == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_LOGE(TAG, "NVS %s", esp_err_to_name(e));
    } else if (e != ESP_OK) {
        ESP_LOGE(TAG, "NVS init %s", esp_err_to_name(e));
    }

    if (bsp_display_init() != ESP_OK) {
        ESP_LOGE(TAG, "display MOSI=%d SCLK=%d CS=%d DC=%d BL=%d",
                 BSP_LCD_MOSI, BSP_LCD_SCLK, BSP_LCD_CS, BSP_LCD_DC, BSP_LCD_BL);
        return;
    }
    bsp_display_backlight(50);
    bsp_button_init(on_key, NULL);
    bsp_pm_init();
    buddy_start();
}
