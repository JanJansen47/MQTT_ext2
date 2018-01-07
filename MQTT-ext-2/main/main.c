#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/event_groups.h"
#include "esp_wifi.h"
#include "esp_system.h"
#include "esp_event.h"
#include "esp_event_loop.h"
#include "nvs_flash.h"
#include "esp_partition.h"
#include "esp_err.h"
#include "nvs.h"
#include "driver/gpio.h"
#include "driver/touch_pad.h"

//#include <esp_log.h>
#include "debug.h"
#include "vars.h"
#include "string.h"
#include "mqtt.h"
#include "bt.h"
#include "sdkconfig.h"
#include "soc/rtc_cntl_reg.h"
#include "soc/sens_reg.h"
#include <stdlib.h>
#include <string.h>
#include "esp_attr.h"
#include <sys/time.h>

#include "esp_event_loop.h"
#include "esp_log.h"
#include "esp_ota_ops.h"
#include <unistd.h>
#include "lwip/err.h"
#include "apps/sntp/sntp.h"
#include <sys/socket.h>
#include <netdb.h>
#include "time.c"
#include "touch.c"
#include "search.c"
#include "freertos/semphr.h"


// include nvs
//#include "nvs_sources.c"
// json

#include "cJSON_Utils.h"
#include <string.h>
#include  <stdio.h>

#include <string.h>
#include <stdio.h>
#include <math.h>
#include <stdlib.h>
#include <limits.h>
#include <ctype.h>


//#include "SPIFFS_sources.c"
//TFT
#include "esp_heap_alloc_caps.h"
#include "tftspi.h"
#include "tft.h"
#include "spiffs_vfs.h"
#include "TFT_sources.c"
#include "vars.h"
#include "mqtt.c"



#define HIGH 1
#define LOW 0
#define UP 1
#define links 1
#define rechts 2

extern void task_disp_menu2(void* p);


// ==================================================
// Define which spi bus to use VSPI_HOST or HSPI_HOST
#define SPI_BUS VSPI_HOST
// ==================================================



QueueHandle_t menu2Queue ;  // communication tussen main en menu2 in TFT_sources
xSemaphoreHandle accessTFT;
//xSemaphoreHandle accessMenu;
xSemaphoreHandle accessmqttcb =1;
xSemaphoreHandle accessdata = 1;
//LCD Touch panel
SemaphoreHandle_t touch = NULL;
#define touch_interrupt_pin 14
#define ESP_INTR_FLAG_DEFAULT 0
#define block 0
#define unblocked 1
bool  int_block = unblocked;

int Temp_buiten, Temp_binnen, Temp_cv;
int Power,Energy, etotal, Pnow, PnowOost, PnowWest, PdagOost, PdagWest;
int MaxPower, MaxPowerDagWest, MaxPowerDagOost, MaxPowerWest, MaxPowerOost;

// digital I/O
#define BLINK_GPIO 4

//ota
#define EXAMPLE_SERVER_IP   CONFIG_SERVER_IP
#define EXAMPLE_SERVER_PORT CONFIG_SERVER_PORT
#define EXAMPLE_FILENAME CONFIG_EXAMPLE_FILENAME
#define BUFFSIZE 1024
#define TEXT_BUFFSIZE 1024
char copie [500]= "{\"name\":\"OPB\",\"punten\":[{\"x\":2,\"y\":18}]}";
char top [100];
static const char *TAG_OTA = "ota";
/*an ota data write buffer ready to write to the flash*/
static char ota_write_data[BUFFSIZE + 1] = { 0 };
/*an packet receive buffer*/
static char text[BUFFSIZE + 1] = { 0 };
/* an image total length*/
static int binary_file_length = 0;
/*socket id*/
static int socket_id = -1;
static char http_request[64] = {0};
static volatile int test_counter = 0;
/* FreeRTOS event group to signal when we are connected & ready to make a request */
static EventGroupHandle_t wifi_event_group;

/* The event group allows multiple bits for each event,
   but we only care about one event - are we connected
   to the AP with an IP? */
const int CONNECTED_BIT = BIT0;



/*read buffer by byte still delim ,return read bytes counts*/
static int read_until(char *buffer, char delim, int len)
{
//  /*TODO: delim check,buffer check,further: do an buffer length limited*/
    int i = 0;
    while (buffer[i] != delim && i < len) {
        ++i;
    }
    return i + 1;
}

/* resolve a packet from http socket
 * return true if packet including \r\n\r\n that means http packet header finished,start to receive packet body
 * otherwise return false
 * */
static bool read_past_http_header(char text[], int total_len, esp_ota_handle_t update_handle)
{
    /* i means current position */
    int i = 0, i_read_len = 0;
    while (text[i] != 0 && i < total_len) {
        i_read_len = read_until(&text[i], '\n', total_len);
        // if we resolve \r\n line,we think packet header is finished
        if (i_read_len == 2) {
            int i_write_len = total_len - (i + 2);
            memset(ota_write_data, 0, BUFFSIZE);
            /*copy first http packet body to write buffer*/
            memcpy(ota_write_data, &(text[i + 2]), i_write_len);

            esp_err_t err = esp_ota_write( update_handle, (const void *)ota_write_data, i_write_len);
            if (err != ESP_OK) {
                ESP_LOGE(TAG, "Error: esp_ota_write failed! err=0x%x", err);
                return false;
            } else {
                ESP_LOGI(TAG, "esp_ota_write header OK");
                binary_file_length += i_write_len;
            }
            return true;
        }
        i += i_read_len;
    }
    return false;
}

static bool connect_to_http_server()
{
    ESP_LOGI(TAG, "Server IP: %s Server Port:%s", EXAMPLE_SERVER_IP, EXAMPLE_SERVER_PORT);
    sprintf(http_request, "GET %s HTTP/1.1\r\nHost: %s:%s \r\n\r\n", EXAMPLE_FILENAME, EXAMPLE_SERVER_IP, EXAMPLE_SERVER_PORT);

    int  http_connect_flag = -1;
    struct sockaddr_in sock_info;

    socket_id = socket(AF_INET, SOCK_STREAM, 0);
    if (socket_id == -1) {
        ESP_LOGE(TAG, "Create socket failed!");
        return false;
    }

    // set connect info
    memset(&sock_info, 0, sizeof(struct sockaddr_in));
    sock_info.sin_family = AF_INET;
    sock_info.sin_addr.s_addr = inet_addr(EXAMPLE_SERVER_IP);
    sock_info.sin_port = htons(atoi(EXAMPLE_SERVER_PORT));

    // connect to http server
    http_connect_flag = connect(socket_id, (struct sockaddr *)&sock_info, sizeof(sock_info));
    if (http_connect_flag == -1) {
        ESP_LOGE(TAG, "Connect to server failed! errno=%d", errno);
        close(socket_id);
        return false;
    } else {
        ESP_LOGI(TAG, "Connected to server");
        return true;
    }
    return false;
}

static void __attribute__((noreturn)) task_fatal_error()
{
    ESP_LOGE(TAG, "Exiting task due to fatal error...");
    close(socket_id);
    (void)vTaskDelete(NULL);

    while (1) {
        ;
    }
}

 static void ota_task()
{
	    esp_err_t err;
    /* update handle : set by esp_ota_begin(), must be freed via esp_ota_end() */
    esp_ota_handle_t update_handle = 0 ;
    const esp_partition_t *update_partition = NULL;

    ESP_LOGI(TAG_OTA, "Starting OTA example...");

    const esp_partition_t *configured = esp_ota_get_boot_partition();
    const esp_partition_t *running = esp_ota_get_running_partition();

    assert(configured == running); /* fresh from reset, should be running from configured boot partition */
    ESP_LOGI(TAG_OTA, "Running partition type %d subtype %d (offset 0x%08x)",
             configured->type, configured->subtype, configured->address);

    /* Wait for the callback to set the CONNECTED_BIT in the
       event group.
    */
   // xEventGroupWaitBits(wifi_event_group, CONNECTED_BIT,
    //                    false, true, portMAX_DELAY);
    ESP_LOGI(TAG_OTA, "Connect to Wifi ! Start to Connect to Server....");

    /*connect to http server*/
    if (connect_to_http_server()) {
        ESP_LOGI(TAG_OTA, "Connected to http server");
    } else {
        ESP_LOGE(TAG_OTA, "Connect to http server failed!");
        task_fatal_error();
    }
    ESP_LOGI(TAG_OTA, "roept ota example");
    int res = -1;
    /*send GET request to http server*/
    res = send(socket_id, http_request, strlen(http_request), 0);
    if (res == -1) {
        ESP_LOGE(TAG_OTA, "Send GET request to server failed");
        task_fatal_error();
    } else {
        ESP_LOGI(TAG_OTA, "Send GET request to server succeeded");
    }

    update_partition = esp_ota_get_next_update_partition(NULL);
    ESP_LOGI(TAG_OTA, "Writing to partition subtype %d at offset 0x%x",
             update_partition->subtype, update_partition->address);
    assert(update_partition != NULL);

    err = esp_ota_begin(update_partition, OTA_SIZE_UNKNOWN, &update_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG_OTA, "esp_ota_begin failed, error=%d", err);
        task_fatal_error();
    }
    ESP_LOGI(TAG_OTA, "esp_ota_begin succeeded");

    bool resp_body_start = false, flag = true;
    /*deal with all receive packet*/
    while (flag) {
        memset(text, 0, TEXT_BUFFSIZE);
        memset(ota_write_data, 0, BUFFSIZE);
        int buff_len = recv(socket_id, text, TEXT_BUFFSIZE, 0);
        ESP_LOGI(TAG_OTA, "buff_len: %d",  buff_len);
        if (buff_len < 0) { /*receive error*/
            ESP_LOGE(TAG, "Error: receive data error! errno=%d", errno);
            task_fatal_error();
        } else if (buff_len > 0 && !resp_body_start) { /*deal with response header*/
            memcpy(ota_write_data, text, buff_len);
            resp_body_start = read_past_http_header(text, buff_len, update_handle);
        } else if (buff_len > 0 && resp_body_start) { /*deal with response body*/
            memcpy(ota_write_data, text, buff_len);
            err = esp_ota_write( update_handle, (const void *)ota_write_data, buff_len);
            if (err != ESP_OK) {
                ESP_LOGE(TAG_OTA, "Error: esp_ota_write failed! err=0x%x", err);
                task_fatal_error();
            }
            binary_file_length += buff_len;
            ESP_LOGI(TAG_OTA, "Have written image length %d   buff_len: %d", binary_file_length, buff_len);
        } else if (buff_len == 0) {  /*packet over*/
            flag = false;
            ESP_LOGI(TAG_OTA, "Connection closed, all packets received");
            close(socket_id);
        } else {
            ESP_LOGE(TAG_OTA, "Unexpected recv result");
        }
    }

    ESP_LOGI(TAG_OTA, "Total Write binary data length : %d", binary_file_length);

    if (esp_ota_end(update_handle) != ESP_OK) {
        ESP_LOGE(TAG, "esp_ota_end failed!");
        task_fatal_error();
    }
    err = esp_ota_set_boot_partition(update_partition);
    if (err != ESP_OK) {
        ESP_LOGE(TAG_OTA, "esp_ota_set_boot_partition failed! err=0x%x", err);
        task_fatal_error();
    }
    ESP_LOGI(TAG_OTA, "Prepare to restart system!");
    esp_restart();
    return ;
}

//eo ota

// NVS handler
nvs_handle my_handle;

// MQTT
static mqtt_client *client;
void mqtt_connected_cb(void *self, void *params)
{
    client = (mqtt_client *)self;
    mqtt_subscribe (client, "/garage/dallas/temp", 0);
    mqtt_subscribe (client, "/RGB/dallas/temp", 0);
    mqtt_subscribe (client, "/kat/dallas/temp", 0);
    mqtt_subscribe (client, "/zonj/power", 0);
    mqtt_subscribe (client, "/zonj/etotal", 0);
    mqtt_subscribe (client, "/zonj/pnow", 0);
    mqtt_subscribe (client, "/zon/power", 0);
    mqtt_subscribe (client, "/zon/etotal", 0);
    mqtt_subscribe (client, "/zon/pnow", 0);
    mqtt_subscribe (client, "/sql/zon",  0);
    mqtt_subscribe (client, "/sql/temp",  0);
    // de maximale waarden van de gemeten zonnepanelen
    mqtt_subscribe (client, "Max_Power", 0);  			// Oost en West  samen op het eind van de dag
    mqtt_subscribe (client, "Power", 0);        		// Oost en West samen loende waarde gedurende de dag
    mqtt_subscribe (client, "Max_Power_Dag_West", 0);  	// max over een dag
    mqtt_subscribe (client, "Max_Power_Dag_Oost", 0);
    mqtt_subscribe (client, "Max_Power_Oost", 0);  		// momentele max waarde
    mqtt_subscribe (client, "Max_Power_West", 0);
    mqtt_subscribe (client, "pnow_West", 0);
    mqtt_subscribe (client, "pnow_Oost", 0);
    mqtt_subscribe (client, "pdag_West", 2);
    mqtt_subscribe (client, "pdag_Oost", 2);
    mqtt_subscribe (client, "/alarm/#", 2);



 //hier kan je publishen
    vTaskDelay(2000 / portTICK_RATE_MS);

    mqtt_publish(client, "Max_Power_info", "1", 6, 2, 0);  // publish a mqtt request 27-07-2017
    mqtt_publish(client, "Max_Power_West_info", "1", 6, 2, 0);  // publish a mqtt request 27-07-2017
    mqtt_publish(client, "Max_Power_Oost_info", "1", 6, 2, 0);  // publish a mqtt request 27-07-2017
    mqtt_publish(client, "Max_Power_Dag_West_info", "1", 6, 2, 0);  // publish a mqtt request 27-07-2017
    mqtt_publish(client, "Max_Power_Dag_Oost_info", "1", 6, 2, 0);  // publish a mqtt request 27-07-2017

}

void mqtt_disconnected_cb(void *self, void *params)
{

}

void mqtt_reconnect_cb(void *self, void *params)
{

}

void mqtt_subscribe_cb(void *self, void *params)
{
	/*
	 * Unsubscribe gaat allen bij het veranderen van het client id.
	 */

}

void mqtt_publish_cb(void *self, void *params)
{

// hier kan je niet publishen.
}


// when ever there's a message, whatever message, coming in through the one subscribed topic, this will trigger

// de berichten worden gecopieerd en in separate taken verwerkt.  Kunnen deze taken de berichten niet bijhouden dan worden er berichten overgeslagen.
void mqtt_data_cb(void *self, void *params)
{
	if (xSemaphoreTake(accessmqttcb, 10000)){
  mqtt_event_data_t *event_data = (mqtt_event_data_t *)params;
  int  lengte_data = event_data->data_length;
  int lengte_topic = event_data->topic_length;
  if ( lengte_data < sizeof(copie) &&  lengte_topic < sizeof(top) ) {

  memcpy(copie,event_data->data,lengte_data);
  copie[lengte_data] = 0;
  memcpy(top,event_data->topic,lengte_topic);
  top[lengte_topic] = 0;
  }

//printf("topic: %s data:   %s \n" ,top, copie);
	}
	xSemaphoreGive(accessmqttcb);
}


mqtt_settings settings = {
    .host = "192.168.0.107",
#if defined(CONFIG_MQTT_SECURITY_ON)
    .port = 8883, // encrypted
#else
    .port = 1883, // unencrypted
#endif
    .client_id = "esp32_27",
    .username = "user",
    .password = "password",
    .clean_session = 0,
    .keepalive = 120,
    .lwt_topic = "/test",
    .lwt_msg = "offline",
    .lwt_qos = 0,
    .lwt_retain = 0,
    .connected_cb = mqtt_connected_cb,
    .disconnected_cb = mqtt_disconnected_cb,
    .reconnect_cb = mqtt_reconnect_cb,
    .subscribe_cb = mqtt_subscribe_cb,
    .publish_cb = mqtt_publish_cb,
    .data_cb = mqtt_data_cb
};

// MQTT


void task_process_mqtt_data(){
#define menu_1 1
#define menu_2 2
int tmp_buff [10];

while(1) {
int menu;
		vTaskDelay(50 / portTICK_RATE_MS);
	if (xSemaphoreTake(accessmqttcb, 1000)){
menu =menu_1;
	if (client) {
		//printf("ingaande string %s \n", top);
		if (search_string("/alarm", top)) {
			   char s[2] = "/";
			   char *token;
			     /* get the first token  "alarm" en skip it*/
			   token = strtok(top, s);

			   strcpy(buf_in_alarm_data.waar, strtok(NULL, s) );
			   strcpy(buf_in_alarm_data.wat,  strtok(NULL, s) );
			   strcpy(buf_in_alarm_data.waarde, copie );
			   buf_in_alarm_data.tijdstip = *localtime(&time_now);
			   if(!xQueueSend(menu2Queue, (void * ) &buf_in_alarm_data, 1500)) {
			   puts("Failed to send item to queue within 1500ms");
			   }
			//menu = menu_2;
		}
	//switch ( menu) {
	//case menu_1: {

	if (search_string("/RGB/dallas/temp", top)) {
		Temp_binnen =atoi(copie);
		printf("Temp_binnen, %d\n", Temp_binnen);
		 }
	if (search_string("/garage/dallas/temp",top)) {
		Temp_buiten =atoi(&copie);
		}
	if (search_string("/kat/dallas/temp", top)) {
		Temp_cv =atoi(&copie);
		}
	if (search_string("/zonj/pnow", top)) {  //27-8 berekening iw * vw + io * vo
		Pnow =atoi(&copie);
		}
	if (search_string("/zonj/power", top)) {  //27-8  diect van Goodwe
		Power =atoi(&copie);
		}
	if (search_string("/zonj/etotal", top)) {  // 27-8 direct van Goodwe
		etotal =atoi(&copie);
		}
	if (search_string("Max_Power", top)) {
		MaxPower =atoi(&copie);
		}
	if (search_string("Max_Power_Dag_Oost", top)) {
		MaxPowerDagOost =atoi(&copie);
		}
	if (search_string("Max_Power_Dag_West", top)) {
		MaxPowerDagWest =atoi(&copie);
		}
	if (search_string("Max_Power_Oost", top)) {
		MaxPowerOost =atoi(&copie);
		}
	if (search_string("Max_Power_West", top)) {
		MaxPowerWest =atoi(&copie);
		}
	if (search_string("Power", top)) {  // uit de database zonnepaneelenergie Eoost + Ewest  is dit het zelfde als /zonj/pnow (zie boven)
		Energy =atoi(&copie);
		}
	if (search_string("pnow_West", top)) {
		PnowWest =atoi(&copie);
		}

	if (search_string("pnow_Oost", top)) {
		PnowOost =atoi(&copie);
		}
	if (search_string("pdag_West", top)) {  // context.global.Opbrengst.West
		PdagWest =atoi(&copie);
			}
	if (search_string("pdag_Oost", top)) {  // context.global.Opbrengst.West
		PdagOost =atoi(&copie);
			}


	if (search_string("/sql/zon", top)) {

	cJSON *root = cJSON_Parse(copie);

		char *naam = cJSON_GetObjectItem(root,"name")->valuestring;
		cJSON *item = cJSON_GetObjectItem(root, "punten");
		    	 int i;
	 	    	 // scaling
		    	 struct Graph_info resultaat;
		    	 for ( i = 0 ; i < cJSON_GetArraySize(item) ; i++)
		    	   {
		    	     cJSON *subitem = cJSON_GetArrayItem(item, i);
		    	     int xc =(int)cJSON_GetObjectItem(subitem, "x")->valueint;
		    	     int yc = (int)cJSON_GetObjectItem(subitem, "y")->valueint;
		    	   //printf("waarden:  %d,   %d\n",  xc, yc);
		    	     resultaat =  Graph_scale( xc,yc);
		           //printf( "scaling: x %d,  %d,  %d, y  %d,  %d,  %d, \n ", resultaat.xlo,resultaat.xhi,resultaat.xinc,resultaat.ylo,resultaat.yhi,resultaat.yinc);
	    	   }
		    	 Graph_reset();

		    	 if (xSemaphoreTake(accessTFT, 1000)){
	 	    	 TFT_fillRect(_width/2,0, _width/2,_height/2, TFT_BLACK );
	    	 Graph(resultaat.xlo,resultaat.ylo, 270,  110, 180,80, resultaat.xlo, resultaat.xhi,   resultaat.xinc,  resultaat.ylo, resultaat.yhi,   resultaat.yinc, naam, "dag", "", TFT_GREEN, TFT_BLACK, TFT_RED, TFT_YELLOW, TFT_WHITE, true);
		    	 xSemaphoreGive(accessTFT);}
		    	 // print de grafiek
		    	 for ( i = 0 ; i < cJSON_GetArraySize(item) ; i++)
		    	   {
		    	      cJSON *subitem = cJSON_GetArrayItem(item, i);
		    	     int xc = (int)cJSON_GetObjectItem(subitem, "x")->valueint;
		    	      int yc = (int)cJSON_GetObjectItem(subitem, "y")->valueint;
		    	      if (xSemaphoreTake(accessTFT, 1000)){
		    	 	 Graph(xc,yc, 270,  110, 180,80, resultaat.xlo, resultaat.xhi,   resultaat.xinc,  resultaat.ylo, resultaat.yhi,   resultaat.yinc, naam, "dag", "", TFT_GREEN, TFT_BLACK, TFT_WHITE, TFT_YELLOW, TFT_WHITE, false);
		    	 	xSemaphoreGive(accessTFT);}
		    	   }
		    	cJSON_Delete (root );
	}


	if (search_string("/sql/temp", top)) {


	cJSON *root = cJSON_Parse(copie);

		char *naam = cJSON_GetObjectItem(root,"name")->valuestring;
		cJSON *item = cJSON_GetObjectItem(root, "punten");
		    	 int i;
	 	    	 // scaling
		    	 struct Graph_info resultaat;
		    	 for ( i = 0 ; i < cJSON_GetArraySize(item) ; i++)
		    	   {
		    	     cJSON *subitem = cJSON_GetArrayItem(item, i);
		    	     int xc = (int)cJSON_GetObjectItem(subitem, "x")->valueint;
		    	     int yc = (int)cJSON_GetObjectItem(subitem, "y")->valueint;
		    	   //printf("waarden:  %d,   %d\n",  xc, yc);
		    	     resultaat =  Graph_scale( xc,yc);
		           //printf( "scaling: x %d,  %d,  %d, y  %d,  %d,  %d, \n ", resultaat.xlo,resultaat.xhi,resultaat.xinc,resultaat.ylo,resultaat.yhi,resultaat.yinc);
	    	   }
		    	 Graph_reset();
		    	 if (xSemaphoreTake(accessTFT, 1000)){
	 	    	 TFT_fillRect(0,0, _width/2,_height/2, TFT_BLACK );
	    	 Graph(resultaat.xlo,resultaat.ylo, 40,  110, 180,80, resultaat.xlo, resultaat.xhi,   resultaat.xinc,  resultaat.ylo, resultaat.yhi,   resultaat.yinc, naam, "dag", "", TFT_GREEN, TFT_BLACK, TFT_RED, TFT_YELLOW, TFT_WHITE, true);
		    	xSemaphoreGive(accessTFT);}
		    	 // print de grafiek
		    	 for ( i = 0 ; i < cJSON_GetArraySize(item) ; i++)
		    	   {
		    	      cJSON *subitem = cJSON_GetArrayItem(item, i);
		    	     int xc = (int)cJSON_GetObjectItem(subitem, "x")->valueint;
		    	      int yc = (int)cJSON_GetObjectItem(subitem, "y")->valueint;
		    	    if (xSemaphoreTake(accessTFT, 1000)){
		    	 	 Graph(xc,yc, 40,  110, 180,80, resultaat.xlo, resultaat.xhi,   resultaat.xinc,  resultaat.ylo, resultaat.yhi,   resultaat.yinc, naam, "dag", "", TFT_GREEN, TFT_BLACK, TFT_WHITE, TFT_YELLOW, TFT_WHITE, false);
		    	 	xSemaphoreGive(accessTFT);}
		    	   }
		    	cJSON_Delete (root );
	} // eo /sql/temp

				memcpy(top,"000000000000",5);  // clean the topic to avoid repeats
			//	break;
	//				} // eo menu_1
	//case menu_2: {
	//	disp_menu2();
	//	menu =menu_1;
	//	break;

	//} // eo menu_2
	//			} // eo switch
			} // eo if client
	    xSemaphoreGive(accessmqttcb);
	   }// eo accessmqtt semaphore taken
	} // eo while forever
}  // eo task


color_t color(int value){
	color_t kleur=TFT_BLUE;
	if (value >=20 && value <25) {kleur = TFT_GREEN; }
	if (value>=25) {kleur = TFT_RED; }
	return kleur;
}

void task_display_mqtt_data(void* p)
{
	color_t kleur;
	char tmp_buff1[7];
	int rouleer = 0;
while(1) {

//binnen
		itoa(Temp_binnen,tmp_buff1,10);   // here 2 means binary
	    update_venster(1,1,"Binnen",tmp_buff1 , color(Temp_binnen));
//buiten
	    itoa(Temp_buiten,tmp_buff1,10);   // here 2 means binary
	    update_venster(1,2,"Buiten",tmp_buff1 , color(Temp_buiten));
//Verwarming
	    itoa(Temp_cv,tmp_buff1,10);   // here 2 means binary
	    update_venster(1,3,"Cv",tmp_buff1 , color(Temp_cv));
	   // printf(" MaxPower :  = %d\n", MaxPower);
	    vTaskDelay(1000 / portTICK_RATE_MS);
//Zon totaal nu  nu
	    if ( rouleer==0) {
	    update_venster(rechts,0,"Zon vandaag"," " ,TFT_WHITE);
	    itoa(Pnow,tmp_buff1,10);   // here 2 means binary    Io*Vo + Iw*Vw
 	    update_venster(2,1,"Nu   W",tmp_buff1 , TFT_GREEN);
	    itoa(Power,tmp_buff1,10);   // here 2 means binary
 	    update_venster(2,2,"Dag kWh",tmp_buff1 , TFT_GREEN);
	    itoa(etotal,tmp_buff1,10);   // here 2 means binary
 	    update_venster(2,3,"Tot  kWh",tmp_buff1 , TFT_GREEN);
 	    rouleer =1;
	    }
	    vTaskDelay(5000 / portTICK_RATE_MS);
//Zon Oost en West in %
	    if ( rouleer==1) {
	    	if (MaxPowerOost !=0 && MaxPowerWest != 0 ){
	    update_venster(rechts,0,"Zon momenteel %"," " ,TFT_WHITE);
	    int result = ((PnowOost*100)/(MaxPowerOost));
	    itoa(result,tmp_buff1,10);   // here 2 means binary
	    update_venster(2,1,"Oost   %",tmp_buff1 , TFT_GREEN);
	   // result = (Energy*100)/MaxPower;
	   // itoa(result,tmp_buff1,10);
	   // update_venster(2,3,"Power  %",tmp_buff1, TFT_GREEN);
	    update_venster(2,3,"","" , _bg);
	    result = ((PnowWest*100)/(MaxPowerWest));
	    itoa(result,tmp_buff1,10);
	    update_venster(2,2,"West   %",tmp_buff1 , TFT_GREEN);

	    	}
	    rouleer =2;
	    	    }
	    vTaskDelay(5000 / portTICK_RATE_MS);
//Zon Oost en West per dag accumulatief
	    if ( rouleer==2) {

	    update_venster(rechts,0,"Zon vandaag"," " ,TFT_WHITE);
	 	itoa(PdagOost,tmp_buff1,10);   // here 2 means binary
	    update_venster(2,1,"Oost  Wh",tmp_buff1 , TFT_GREEN);
	    update_venster(2,3,"","" , _bg);
	    itoa(PdagWest,tmp_buff1,10);
	    update_venster(2,2,"West  Wh",tmp_buff1 , TFT_GREEN);


	    rouleer =3;
	    	    }
	    vTaskDelay(5000 / portTICK_RATE_MS);

// Euros
	     if ( rouleer==3) {
	    	 float Euro = 0.22;
	     update_venster(rechts,0,"Euro's"," " ,TFT_WHITE);
	     update_venster(2,1,"","" , _bg);
	     snprintf (tmp_buff1, sizeof(tmp_buff1-1), "%f",Power*Euro);
	     update_venster(2,2,"Dag $",tmp_buff1 , TFT_GREEN);
	     snprintf (tmp_buff1, sizeof(tmp_buff1), "%f",etotal*Euro);  // pas gaat fout over 7 jaar
	     update_venster(2,3,"Tot  $",tmp_buff1 , TFT_GREEN);
	 	 rouleer =4;
	    	    }
	 	  vTaskDelay(5000 / portTICK_RATE_MS);

//Zon Oost en West pieken
	    if ( rouleer==4) {
	    update_venster(rechts,0,"Zon piek "," " ,TFT_WHITE);
	    itoa(MaxPowerOost,tmp_buff1,10);   // here 2 means binary
	    update_venster(2,1,"Oost   W",tmp_buff1 , TFT_GREEN);
	    itoa(MaxPowerWest,tmp_buff1,10);
	    update_venster(2,2,"West   W",tmp_buff1 , TFT_GREEN);
	    update_venster(2,3,""," ", _bg);  // wis deze lijn
	    rouleer =5;
	    	    }
	    vTaskDelay(5000 / portTICK_RATE_MS);

 //Zon Oost en West maxima
        if ( rouleer==5) {
	    update_venster(rechts,0,"Zon piek dag"," " ,TFT_WHITE);
	    itoa(MaxPowerDagOost,tmp_buff1,10);   // here 2 means binary
	    update_venster(2,1,"Oost kWh",tmp_buff1 , TFT_GREEN);
	    itoa(MaxPowerDagWest,tmp_buff1,10);
	    update_venster(2,2,"West kWh",tmp_buff1 , TFT_GREEN);
	    itoa(MaxPower,tmp_buff1,10);
	    update_venster(2,3,"Tot   kWh",tmp_buff1, TFT_WHITE);
	    rouleer =0;
	    vTaskDelay(5000 / portTICK_RATE_MS);
	    	    	    }
		}
}

// WIFI

static esp_err_t wifi_event_handler(void *ctx, system_event_t *event)
{
	wifi_event_group = xEventGroupCreate();
	tcpip_adapter_init();
    switch(event->event_id) {
    case SYSTEM_EVENT_STA_START:
        ESP_ERROR_CHECK(esp_wifi_connect());
        break;

    case SYSTEM_EVENT_STA_GOT_IP:
        // start mqtt...
        mqtt_start(&settings);
      // initialize_sntp(); // juni time
     //  obtain_time();
      //  xEventGroupSetBits(wifi_event_group, CONNECTED_BIT);
        // Notice that, all callback will called in mqtt_task
        // All function publish, subscribe
        break;
    case SYSTEM_EVENT_STA_DISCONNECTED:
        /* This is a workaround as ESP32 WiFi libs don't currently
           auto-reassociate. */
       // INFO("WIFI disconnected...\n");
        mqtt_stop();
        ESP_ERROR_CHECK(esp_wifi_connect());
      //  xEventGroupClearBits(wifi_event_group, CONNECTED_BIT);
        break;
    default:
        break;
    }
    return ESP_OK;
}

void wifi_conn_init(void)
{

}



void IRAM_ATTR touch_isr_handler(void* arg) {
 // stuur bericht naar touch taak
	if (int_block == unblocked) {
	xSemaphoreGiveFromISR( touch, NULL);}
}

int  checkfield (int field,int xc, int yc, int x, int y, int w, int h){
//int result =0;
//	printf( "stap 1:: xc > x && xc < (x+ w): %d \n " , (xc > x && xc < (x+ w)  ));
//	printf( "stap 1:: yc > y && yc <( y+h): %d \n " , (yc > y && yc <( y+h)));
//    printf( "xc: %d x: %d x+w: %d     yc: %d y:  %d  y+h:  %d \n", xc,x,x+w, yc, y, y+h);
if (xc > x && xc < (x+ w) && yc > y && yc <( y+h)) {
//	printf( "xc > x && xc < (x+ w): %d \n " , (xc > x && xc < (x+ w)  ));
//	printf( "yc > y && yc <( y+h): %d \n " , (yc > y && yc <( y+h)));
//	printf(" field:  %d \n", field);
	return field;
}
//printf(" field bij return 0:  %d \n", field);
return 0;

}

int checktouchfield (int x, int y){
TFT_setFont(2, NULL);
int field =checkfield(6, x,y, 1, _height-TFT_getfontheight()-8, _width/3-3, TFT_getfontheight()+16);
if (field != 0) return field;
int line = 0;
for (line=1; line < 4; line++){
field =checkfield(line, x,y,10,line*(TFT_getfontheight()*1.3)+10, _width/2-12,TFT_getfontheight()+12);
if (field >0) {
if (xSemaphoreTake(accessTFT, 1000)){
TFT_saveClipWin();
TFT_resetclipwin();

TFT_fillCircle(x, y, 7,TFT_BLUE);
TFT_restoreClipWin();
xSemaphoreGive(accessTFT);
return field;
} } }
return 0;
}

void LCD_touch_task(void* arg) {

	// infinite loop
	for(;;) {

		// wait for the notification from the ISR
		if(xSemaphoreTake(touch,portMAX_DELAY) ==pdTRUE) {  //pdTRUE
			int_block = block;
			int tx, ty;
			TFT_read_touch(&tx, &ty, 0);
			printf( "touch field:  %d \n", checktouchfield (tx,ty));
			vTaskDelay(10 / portTICK_RATE_MS);
			int_block = unblocked;
		}
	}
}

void app_main(void)
{
		// initialise digital I/O
		gpio_pad_select_gpio(BLINK_GPIO);
		gpio_set_direction(BLINK_GPIO, GPIO_MODE_OUTPUT);
		gpio_set_level(BLINK_GPIO, 0);
		//Mutex voor toegang tot het display
		accessTFT = 1;
		accessTFT = xSemaphoreCreateMutex();
		//Mutex voor het uitlezen van de mqtt callback
		accessmqttcb = 1;
		accessmqttcb = xSemaphoreCreateMutex();
		// voor LCD touch interrupt
		touch = xSemaphoreCreateBinary();
		//voor menu selectie
		//accessMenu = 1;
		//accessMenu =  xSemaphoreCreateMutex();
		menu2Queue = xQueueCreate(5, sizeof(struct alarm_data));

		//gpio_pad_select_gpio(touch_interrupt_pin);
		gpio_set_direction(touch_interrupt_pin, GPIO_MODE_INPUT);
		gpio_pullup_en(touch_interrupt_pin);
		// enable interrupt (1->0) voor het touch scherm
		gpio_set_intr_type(touch_interrupt_pin, GPIO_INTR_NEGEDGE);
		// noodzakelijk voor interrupt van GPIO pin
		gpio_install_isr_service(ESP_INTR_FLAG_DEFAULT);
		//koppel de default met de touch interrupt pin
		gpio_isr_handler_add(touch_interrupt_pin, touch_isr_handler, NULL);

	    ESP_ERROR_CHECK( nvs_flash_init() );

// Wifi Connect

	    init_TFT();  // initialiseer TFT scherm
	    disp_header("In 'Main'");
	    update_header( "TFT started",1,_bg);
	    vTaskDelay(500 / portTICK_RATE_MS);
	    tcpip_adapter_init();

	    ESP_ERROR_CHECK( esp_event_loop_init(wifi_event_handler, NULL) );

	    wifi_init_config_t icfg = WIFI_INIT_CONFIG_DEFAULT();
	    ESP_ERROR_CHECK( esp_wifi_init(&icfg) );
	    ESP_ERROR_CHECK( esp_wifi_set_storage(WIFI_STORAGE_RAM) );

	    wifi_config_t wifi_config = {
	        .sta = {
	            .ssid = "JOKER-up",
	            .password = "ABCDABCDABCDABCDABCDABCDAB",
	            .bssid_set = false
	        },
	    };

	    ESP_ERROR_CHECK( esp_wifi_set_mode(WIFI_MODE_STA));
	    ESP_ERROR_CHECK( esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
	    ESP_ERROR_CHECK( esp_wifi_start());
	    update_header( "WiFi ok ", 1, _bg);
	    vTaskDelay(500 / portTICK_RATE_MS);

    // Initialize touch pad peripheral
    ESP_LOGI(TAG, "Initializing touch pad");
    touch_pad_init();
    tp_example_set_thresholds();
    touch_pad_isr_handler_register(tp_example_rtc_intr, NULL, 0, NULL);

    //xTaskCreate(&tp_example_read_task_int, "touch_pad_read_task", 2048, NULL, 5, NULL);
    xTaskCreate(&task_display_mqtt_data, "display mgtt data_task", 2048, NULL, 5, NULL);

    xTaskCreate(&disp_menu2, "test22", 2048, NULL, 5, NULL);

    xTaskCreate(&LCD_touch_task, "LCD_touch_task", 2048, NULL, 5, NULL);
    xTaskCreate(&task_process_mqtt_data, "verwerk mqtt berichten", 2048,NULL,5,NULL);
    int counter1=0;
     vTaskDelay(10000 / portTICK_RATE_MS);  // proef ondervindelijk anders is de klok nog niet gestart.
    checkTime();
    time_t start_t, end_t;
    time(&start_t);

//show startup in database
    test(client,"45");

    while (true) {
    	counter1++;
// difference time counter
    	time(&end_t);
    	int tijd  = (int)difftime(end_t, start_t);
    	if (tijd < 360)    {sprintf(tmp_buff, "Up:%d  sec",tijd);}
    	if (tijd > 360 && tijd < 7200)  {tijd =tijd/60; sprintf(tmp_buff, "Up:%d min" , tijd);}
    	if (tijd >= 7200) {tijd =tijd/3600; sprintf(tmp_buff, "Up:%d uur", tijd);}
        update_header( tmp_buff, 1, _bg);

// toon datum
        checkTime();
        tm_info = localtime(&time_now);
    	sprintf(tmp_buff, "%02d:%02d:%02d", tm_info->tm_hour, tm_info->tm_min, tm_info->tm_sec);
    	printf("%02d:%02d:%02d \n", tm_info->tm_hour, tm_info->tm_min, tm_info->tm_sec);
    	update_header(tmp_buff,2,_bg);
// ota nodig??
    	//if (counter1==10) {test(client, "01");vTaskDelay(10000 / portTICK_RATE_MS); ota_task();  }



    	vTaskDelay(500 / portTICK_RATE_MS);
    	gpio_set_level(BLINK_GPIO, 1);
       	update_header("V",3,TFT_GREEN);
    	vTaskDelay(300 / portTICK_RATE_MS);
    	update_header("A",4,TFT_PINK);
    	update_header( "v45", 1, _bg);
    	vTaskDelay(300 / portTICK_RATE_MS);
    	update_header("A",5,TFT_YELLOW);
    	vTaskDelay(300 / portTICK_RATE_MS);
    	update_header("V",6,TFT_BLUE);
    	vTaskDelay(1000 / portTICK_RATE_MS);

    	sprintf(tmp_buff, "%d", counter1);
    	update_header(tmp_buff,1,_bg);
    	update_header("6",6,TFT_RED);
    	vTaskDelay(300 / portTICK_RATE_MS);
    	update_header("5",5,TFT_RED);
    	vTaskDelay(300 / portTICK_RATE_MS);
    	update_header("4",4,TFT_RED);
    	vTaskDelay(300 / portTICK_RATE_MS);
    	update_header("3",3,TFT_RED);
    	gpio_set_level(BLINK_GPIO, 0);
       	vTaskDelay(1000 / portTICK_RATE_MS);


    }
}

