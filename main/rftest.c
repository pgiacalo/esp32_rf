#include <math.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "driver/rmt_tx.h"
#include "driver/dac_oneshot.h"

#define M_PI 3.14159265358979323846

// Global handle for DAC
static dac_oneshot_handle_t dac_handle;

// Task for DAC output
void dac_task(void *pvParameters)
{
    // Generate sine wave on DAC
    while (1) {
        for(int i = 0; i < 256; i++) {
            uint8_t value = 128 + 127 * sin(2 * M_PI * i / 256.0);
            dac_oneshot_output_voltage(dac_handle, value);
            vTaskDelay(1);
        }
        vTaskDelay(1);
    }
}

void app_main(void)
{
    // Configure RMT TX channel
    rmt_tx_channel_config_t tx_chan_config = {
        .clk_src = RMT_CLK_SRC_DEFAULT,
        .gpio_num = 18,
        .mem_block_symbols = 64,
        .resolution_hz = 1000000, // Reduced to 1MHz for cleaner timing
        .trans_queue_depth = 4,
        .flags.with_dma = false,
    };
    
    rmt_channel_handle_t tx_chan = NULL;
    ESP_ERROR_CHECK(rmt_new_tx_channel(&tx_chan_config, &tx_chan));

    // Configure carrier - using 100kHz as an example
    rmt_carrier_config_t carrier_config = {
        .duty_cycle = 0.5,
        .frequency_hz = 100000, // Reduced to 100kHz for cleaner output
        .flags.polarity_active_low = false,
    };
    ESP_ERROR_CHECK(rmt_apply_carrier(tx_chan, &carrier_config));

    // Create and initialize copy encoder
    rmt_encoder_handle_t copy_encoder = NULL;
    rmt_copy_encoder_config_t copy_encoder_config = {};
    ESP_ERROR_CHECK(rmt_new_copy_encoder(&copy_encoder_config, &copy_encoder));

    // Configure DAC
    dac_oneshot_config_t dac_config = {
        .chan_id = DAC_CHAN_0, // GPIO25
    };
    ESP_ERROR_CHECK(dac_oneshot_new_channel(&dac_config, &dac_handle));

    // Create a simple pattern for RMT with longer durations
    rmt_symbol_word_t pattern[2] = {
        {.duration0 = 500, .level0 = 1, .duration1 = 500, .level1 = 0},
        {.duration0 = 0, .level0 = 0, .duration1 = 0, .level1 = 0}
    };

    // Enable the transmitter
    ESP_ERROR_CHECK(rmt_enable(tx_chan));

    // Transmit configuration
    rmt_transmit_config_t tx_config = {
        .loop_count = -1, // Infinite loop
    };

    // Start transmission
    ESP_ERROR_CHECK(rmt_transmit(tx_chan, copy_encoder, pattern, sizeof(pattern), &tx_config));

    // Create DAC task
    xTaskCreate(dac_task, "dac_task", 2048, NULL, 5, NULL);

    // Main task just needs to periodically yield
    while(1) {
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}