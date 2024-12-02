#include "led.h"

#include "mdf_common.h"

#include "driver/rmt.h"
#include "led_strip.h"
#include "globals.h"

led_strip_t *strip;
int64_t lastLedUpdate = -ledTimeout;    // initialize value so LEDs start in timeout

void ledInit() {
    rmt_config_t config = RMT_DEFAULT_CONFIG_TX(LED_PIN, RMT_TX_CHANNEL);
    // set counter clock to 40MHz
    config.clk_div = 2;

    ESP_ERROR_CHECK(rmt_config(&config));
    ESP_ERROR_CHECK(rmt_driver_install(config.channel, 0, 0));

    // install ws2812 driver
    led_strip_config_t strip_config = LED_STRIP_DEFAULT_CONFIG(LED_NUM, (led_strip_dev_t)config.channel);
    strip = led_strip_new_rmt_ws2812(&strip_config);
    if (!strip) {
        ESP_LOGE(TAG, "install WS2812 driver failed");
    }
    // Clear LED strip (turn off all LEDs)
    ESP_ERROR_CHECK(strip->clear(strip, 100));

    // test data
    ESP_ERROR_CHECK(strip->set_pixel(strip, 0, 64, 0, 0));
    ESP_ERROR_CHECK(strip->set_pixel(strip, 1, 0, 64, 0));
    ESP_ERROR_CHECK(strip->set_pixel(strip, 2, 0, 0, 64));
    ESP_ERROR_CHECK(strip->set_pixel(strip, 3, 64, 64, 64));
    ESP_ERROR_CHECK(strip->refresh(strip, 100));

}

void ledSetData(uint8_t *data, size_t len) {
    if (strip) {
        lastLedUpdate = esp_timer_get_time();
        for (int i = 0; i < len; i += 3) {
            int pixelIdx = i / 3;
            // r->g
            // g->b
            // b->r
            ESP_ERROR_CHECK(strip->set_pixel(strip, pixelIdx, data[i+1], data[i+2], data[i]));
        }
        ESP_ERROR_CHECK(strip->refresh(strip, 100));
    }
}

void ledLoop() {
    if (esp_timer_get_time() - lastLedUpdate > ledTimeout) {
        if (strip) {
            strip->clear(strip, 100);
        }
    }
}