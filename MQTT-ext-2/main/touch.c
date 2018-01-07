#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "soc/rtc_cntl_reg.h"
#include "soc/sens_reg.h"
#include "driver/gpio.h"
#include "driver/touch_pad.h"
#include <esp_log.h>

extern void checkTime(void);
extern uint8_t temprature_sens_read(void); // from librtc.a in c file
static const char* TAG = "Touch pad";
static bool s_pad_activated[TOUCH_PAD_MAX];


static void tp_example_set_thresholds(void)
{
    uint16_t touch_value;
    for (int i=8; i<TOUCH_PAD_MAX; i++) {  // mod 29-5  overlap met SPI LCD
        ESP_ERROR_CHECK(touch_pad_read(i, &touch_value));
        ESP_ERROR_CHECK(touch_pad_config(i, touch_value/2));
    }
}

/*
  Check if any of touch pads has been activated
  by reading a table updated by rtc_intr()
  If so, then print it out on a serial monitor.
  Clear related entry in the table afterwards
 */
static void tp_example_read_task_int(void *pvParameter)
{
    static int show_message;
    while (1) {
        for (int i=8; i<TOUCH_PAD_MAX; i++) {  // mod 29-5
            if (s_pad_activated[i] == true) {
                ESP_LOGI(TAG, "T%d activated!", i);
                // Wait a while for the pad being released
                vTaskDelay(200 / portTICK_PERIOD_MS);
                //obtain_time();
                //checkTime();
                // Clear information on pad activation
                uint8_t temp =    temprature_sens_read();

                ESP_LOGI(TAG, "  %f temperatuur: ", (temp -32)/1.800);
                s_pad_activated[i] = false;
                // Reset the counter triggering a message
                // that application is running
                show_message = 1;
            }
        }
        // If no pad is touched, every couple of seconds, show a message
        // that application is running
        if (show_message++ % 500 == 0) {
           // ESP_LOGI(TAG, "Waiting for any pad being touched...");
        }
        vTaskDelay(10 / portTICK_PERIOD_MS);
    }
}
/*
  Handle an interrupt triggered when a pad is touched.
  Recognize what pad has been touched and save it in a table.
 */
static void tp_example_rtc_intr(void * arg)
{
    uint32_t pad_intr = READ_PERI_REG(SENS_SAR_TOUCH_CTRL2_REG) & 0x3ff;
    uint32_t rtc_intr = READ_PERI_REG(RTC_CNTL_INT_ST_REG);
    //clear interrupt
    WRITE_PERI_REG(RTC_CNTL_INT_CLR_REG, rtc_intr);
    SET_PERI_REG_MASK(SENS_SAR_TOUCH_CTRL2_REG, SENS_TOUCH_MEAS_EN_CLR);

    if (rtc_intr & RTC_CNTL_TOUCH_INT_ST) {
        for (int i = 8; i < TOUCH_PAD_MAX; i++) { // mod 29-5
            if ((pad_intr >> i) & 0x01) {
                s_pad_activated[i] = true;
            }
        }
    }
}
