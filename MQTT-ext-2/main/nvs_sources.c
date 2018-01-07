/*
 * nvs_sources.c
 *
 *  Created on: Jul 9, 2017
 *      Author: jan

#include "nvs_flash.h"
#include "/home/jan/esp/esp-idf/components/spi_flash/include/esp_partition.h"
#include "/home/jan/esp/esp-idf/components/esp32/include/esp_err.h"
//#include "esp_err.h"
#include "nvs.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include <errno.h>

//#include "TFT_sources.c"

nvs_handle my_handle;

void nvs_init_jan (void) {

	// test nvs
		// initialize NVS flash

			esp_err_t err = nvs_flash_init();
			// NVS handler

			// if it is invalid, try to erase it
		    if (err == ESP_ERR_NVS_NO_FREE_PAGES) {

				printf("Got NO_FREE_PAGES error, trying to erase the partition...\n");

				// find the NVS partition
		        const esp_partition_t* nvs_partition = esp_partition_find_first(ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_DATA_NVS, NULL);
				if(!nvs_partition) {

					printf("FATAL ERROR: No NVS partition found\n");
					while(1) vTaskDelay(10 / portTICK_PERIOD_MS);
				}

				// erase the partition
		        err = (esp_partition_erase_range(nvs_partition, 0, nvs_partition->size));
				if(err != ESP_OK) {
					printf("FATAL ERROR: Unable to erase the partition\n");
					while(1) vTaskDelay(10 / portTICK_PERIOD_MS);
				}
				printf("Partition erased!\n");

				// now try to initialize it again
				err = nvs_flash_init();
				if(err != ESP_OK) {

					printf("FATAL ERROR: Unable to initialize NVS\n");
					while(1) vTaskDelay(10 / portTICK_PERIOD_MS);
				}
			}
			printf("NVS init OK!\n");
			//update_header(NULL, "NVS init OK!!" );
			vTaskDelay(1000 / portTICK_PERIOD_MS);
			// open the partition in RW mode
			err = nvs_open("storJan", NVS_READWRITE, &my_handle);
		    if (err != ESP_OK) {

				printf("FATAL ERROR: Unable to open NVS\n");
				while(1) vTaskDelay(10 / portTICK_PERIOD_MS);
		}

		    printf("NVS open OK\n");
		    update_header(NULL, "NVS OPEN!!" );
		    vTaskDelay(1000 / portTICK_PERIOD_MS);
	    // eo test nvs

}
*/
