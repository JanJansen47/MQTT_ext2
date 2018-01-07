#include "soc/rtc_cntl_reg.h"
#include "soc/sens_reg.h"
#include <stdlib.h>
#include <string.h>
#include "esp_attr.h"
#include <sys/time.h>
#include <unistd.h>
#include "lwip/err.h"
#include "apps/sntp/sntp.h"
#include <esp_log.h>

/*
 * Deze functie leest de ntp tijd voor europe / amsterdam
 */

//time
 time_t time_now, time_last = 0;
struct tm* tm_info;
static char tmp_buff[64];
static const char tag[] = "[Time ntp]";
//-------------------------------
static void initialize_sntp(void)
{
    ESP_LOGI(tag, "Initializing SNTP");
    sntp_setoperatingmode(SNTP_OPMODE_POLL);
    sntp_setservername(0, "pool.ntp.org");
    sntp_init();
    }

//--------------------------

static int obtain_time(void)
{
	int res = 1;

	// Set time zone
	setenv("TZ", "CET-1CEST", 0);  // zie afkortingen http://www.remotemonitoringsystems.ca/time-zone-abbreviations.php
	tzset();

    // wait for time to be set
    int retry = 0;
    const int retry_count = 20;

    time(&time_now);
	tm_info = localtime(&time_now);

    while(tm_info->tm_year < (2016 - 1900) && ++retry < retry_count) {
        ESP_LOGI(tag, "Waiting for system time to be set... (%d/%d)", retry, retry_count);
		sprintf(tmp_buff, "Wait %0d/%d", retry, retry_count);
    	//TFT_print(tmp_buff, CENTER, LASTY);
		vTaskDelay(500 / portTICK_RATE_MS);
        time(&time_now);
    	tm_info = localtime(&time_now);
    }
    if (tm_info->tm_year < (2016 - 1900)) {
    	ESP_LOGI(tag, "System time NOT set.");
    	res = 0;
    }
    else {
    	ESP_LOGI(tag, "System time is set.");
    }

   // ESP_ERROR_CHECK( esp_wifi_stop() );
    return res;
}

//----------------------

static void checkTime(void)
{	char buf [8];
	time(&time_now);
	if (time_now > time_last) {
	//	color_t last_fg, last_bg;
		time_last = time_now;
		tm_info = localtime(&time_now);



	}

}

