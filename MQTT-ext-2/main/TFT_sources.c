/*
 * TFT_sources.c
 *
 *  Created on: Jun 19, 2017
 *      Author: jan
 */

#include "SPIFFS_sources.c"
//TFT
#include "esp_heap_alloc_caps.h"
#include "tftspi.h"
#include "tft.h"
#include "spiffs_vfs.h"
#include "vars.h"

//---------------------------------
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "cJSON_Utils.h"
#include "cJSON.h"


#define links  1
#define rechts 2
// ==================================================
// Define which spi bus to use VSPI_HOST or HSPI_HOST
#define SPI_BUS VSPI_HOST
// ==================================================
struct alarm_data alarms[10];

static void init_TFT()  {


	    // ========  PREPARE DISPLAY INITIALIZATION  =========

	    esp_err_t ret;

	    // === SET GLOBAL VARIABLES ==========================

	    // ===================================================
	    // ==== Set display type                         =====
	    //tft_disp_type = DEFAULT_DISP_TYPE;
		//tft_disp_type = DISP_TYPE_ILI9341;
		tft_disp_type = DISP_TYPE_ILI9488;
	    // ===================================================

		// ===================================================
		// === Set display resolution if NOT using default ===
		// === DEFAULT_TFT_DISPLAY_WIDTH &                 ===
	    // === DEFAULT_TFT_DISPLAY_HEIGHT                  ===
		_width = DEFAULT_TFT_DISPLAY_WIDTH;  // smaller dimension
		_height = DEFAULT_TFT_DISPLAY_HEIGHT; // larger dimension
		// ===================================================

		// ===================================================
		// ==== Set maximum spi clock for display read    ====
		//      operations, function 'find_rd_speed()'    ====
		//      can be used after display initialization  ====
		max_rdclock = 8000000;
		// ===================================================


	    // ====  CONFIGURE SPI DEVICES(s)  ====================================================================================

	    gpio_set_direction(PIN_NUM_MISO, GPIO_MODE_INPUT);
	    gpio_set_pull_mode(PIN_NUM_MISO, GPIO_PULLUP_ONLY);

	    spi_lobo_device_handle_t spi;

	    spi_lobo_bus_config_t buscfg={
	        .miso_io_num=PIN_NUM_MISO,				// set SPI MISO pin
	        .mosi_io_num=PIN_NUM_MOSI,				// set SPI MOSI pin
	        .sclk_io_num=PIN_NUM_CLK,				// set SPI CLK pin
	        .quadwp_io_num=-1,
	        .quadhd_io_num=-1,
			.max_transfer_sz = 6*1024,
	    };
	    spi_lobo_device_interface_config_t devcfg={
	        .clock_speed_hz=8000000,                // Initial clock out at 8 MHz
	        .mode=0,                                // SPI mode 0
	        .spics_io_num=-1,                       // we will use external CS pin
			.spics_ext_io_num=PIN_NUM_CS,           // external CS pin
			.flags=SPI_DEVICE_HALFDUPLEX,           // ALWAYS SET  to HALF DUPLEX MODE!! for display spi
	    };

	#if USE_TOUCH
	    spi_lobo_device_handle_t tsspi = NULL;

	    spi_lobo_device_interface_config_t tsdevcfg={
	        .clock_speed_hz=2500000,                //Clock out at 2.5 MHz
	        .mode=0,                                //SPI mode 2
	        .spics_io_num=PIN_NUM_TCS,              //Touch CS pin
			.spics_ext_io_num=-1,                   //Not using the external CS
			.command_bits=8,                        //1 byte command
	    };
	#endif
	    // ====================================================================================================================



		vTaskDelay(500 / portTICK_RATE_MS);
		printf("\r\n==============================\r\n");
	    printf("TFT display DEMO, LoBo 07/2017\r\n");
		printf("==============================\r\n\r\n");

		// ==================================================================
		// ==== Initialize the SPI bus and attach the LCD to the SPI bus ====

		ret=spi_lobo_bus_add_device(SPI_BUS, &buscfg, &devcfg, &spi);
	    assert(ret==ESP_OK);
		printf("SPI: display device added to spi bus (%d)\r\n", SPI_BUS);
		disp_spi = spi;

		// ==== Test select/deselect ====
		ret = spi_lobo_device_select(spi, 1);
	    assert(ret==ESP_OK);
		ret = spi_lobo_device_deselect(spi);
	    assert(ret==ESP_OK);

		printf("SPI: attached display device, speed=%u\r\n", spi_lobo_get_speed(spi));
		printf("SPI: bus uses native pins: %s\r\n", spi_lobo_uses_native_pins(spi) ? "true" : "false");

	#if USE_TOUCH
		// =====================================================
	    // ==== Attach the touch screen to the same SPI bus ====

		ret=spi_lobo_bus_add_device(SPI_BUS, &buscfg, &tsdevcfg, &tsspi);
	    assert(ret==ESP_OK);
		printf("SPI: touch screen device added to spi bus (%d)\r\n", SPI_BUS);
		ts_spi = tsspi;

		// ==== Test select/deselect ====
		ret = spi_lobo_device_select(tsspi, 1);
	    assert(ret==ESP_OK);
		ret = spi_lobo_device_deselect(tsspi);
	    assert(ret==ESP_OK);

		printf("SPI: attached TS device, speed=%u\r\n", spi_lobo_get_speed(tsspi));
	#endif

		// ================================
		// ==== Initialize the Display ====

		printf("SPI: display init...\r\n");

	//	while (1){
	// 		printf("in de loop");}
		TFT_display_init();
	    printf("OK\r\n");

		// ==== Set SPI clock used for display operations ====
		spi_lobo_set_speed(spi, DEFAULT_SPI_CLOCK);
		printf("SPI: Changed speed to %u\r\n", spi_lobo_get_speed(spi));

		printf("\r\n---------------------\r\n");
		printf("Graphics demo started\r\n");
		printf("---------------------\r\n");

		font_rotate = 0;
		text_wrap = 0;
		font_transparent = 0;
		font_forceFixed = 0;
		gray_scale = 0;
	    TFT_setGammaCurve(DEFAULT_GAMMA_CURVE);
		TFT_setRotation(LANDSCAPE);
		TFT_setFont(DEFAULT_FONT, NULL);
		TFT_resetclipwin();


}

static void disp_header(char *info)

{
	if (xSemaphoreTake(accessTFT, 1000)){

	TFT_fillScreen(TFT_BLACK);
	TFT_resetclipwin();

	_fg = TFT_YELLOW;
	_bg = (color_t){ 64, 64, 64 };

	TFT_setFont(DEFAULT_FONT, NULL);
	//temperatuur vlak links
	TFT_fillRoundRect(0, 0, _width/2-1, (320-TFT_getfontheight()+8)/2 -25,5, _bg); // pas op de de x en y na de middelste comma zijn relatief
	TFT_drawRoundRect(0, 0, _width/2-1, (320-TFT_getfontheight()+8)/2 -25,5, TFT_CYAN);
	TFT_setFont(2, NULL);
	_fg = TFT_WHITE;
	TFT_print("Temperaturen", 10, 2);
	_fg = TFT_YELLOW;
	TFT_setFont(DEFAULT_FONT, NULL);
	// Zon vlak rechts
	TFT_fillRoundRect(_width/2, 0, _width/2-1, (320-TFT_getfontheight()+8)/2-25,5, _bg);
	TFT_drawRoundRect(_width/2, 0, _width/2-1, (320-TFT_getfontheight()+8)/2-25,5, TFT_CYAN);
	TFT_setFont(2, NULL);
	TFT_setFont(DEFAULT_FONT, NULL);

	// bottom mededelingen
	TFT_setFont(1, NULL);
	TFT_fillRoundRect(0, _height-TFT_getfontheight()-9, _width/3-1, TFT_getfontheight()+8,5, _bg);
	TFT_drawRoundRect(0, _height-TFT_getfontheight()-9, _width/3-1, TFT_getfontheight()+8,5, TFT_CYAN);


	// bottom klokje
	TFT_fillRoundRect(_width/3 +10, _height-TFT_getfontheight()-9, _width/3-31, TFT_getfontheight()+8,5, _bg);
	TFT_drawRoundRect(_width/3 +10, _height-TFT_getfontheight()-9, _width/3-31, TFT_getfontheight()+8,5, TFT_CYAN);

	// 3

	TFT_fillRect(_width/3 +10 + _width/3 -31 +  11 , _height-TFT_getfontheight()-8, (_width-(_width/3 +10 + _width/3 +31))/4 , TFT_getfontheight()+6, TFT_GREEN);
	TFT_drawRect(_width/3 +10 + _width/3 -31 +  11 , _height-TFT_getfontheight()-8, (_width-(_width/3 +10 + _width/3 +31))/4 , TFT_getfontheight()+6, TFT_WHITE);

	// 4
	TFT_fillRect(_width/3 +10 + _width/3 -31 +  2*11 + (_width-(_width/3 +10 + _width/3 +31))/4 , _height-TFT_getfontheight()-8, (_width-(_width/3 +10 + _width/3 +31))/4 , TFT_getfontheight()+6, TFT_GREEN);
	TFT_drawRect(_width/3 +10 + _width/3 -31 +  2*11 + (_width-(_width/3 +10 + _width/3 +31))/4 , _height-TFT_getfontheight()-8, (_width-(_width/3 +10 + _width/3 +31))/4 , TFT_getfontheight()+6, TFT_WHITE);

	// 5
	TFT_fillRect(_width/3 +10 + _width/3 -31 +  3*11 + 2*(_width-(_width/3 +10 + _width/3 +31))/4, _height-TFT_getfontheight()-8, (_width-(_width/3 +10 + _width/3 +31))/4 , TFT_getfontheight()+6, TFT_GREEN);
	TFT_drawRect(_width/3 +10 + _width/3 -31 +  3*11 + 2*(_width-(_width/3 +10 + _width/3 +31))/4, _height-TFT_getfontheight()-8, (_width-(_width/3 +10 + _width/3 +31))/4 , TFT_getfontheight()+6, TFT_WHITE);


	//6
	TFT_fillRect(_width/3 +10 + _width/3 -31 +  4*11 + 3*(_width-(_width/3 +10 + _width/3 +31))/4, _height-TFT_getfontheight()-8, (_width-(_width/3 +10 + _width/3 +31))/4 , TFT_getfontheight()+6, TFT_GREEN);
	TFT_drawRect(_width/3 +10 + _width/3 -31 +  4*11 + 3*(_width-(_width/3 +10 + _width/3 +31))/4, _height-TFT_getfontheight()-8, (_width-(_width/3 +10 + _width/3 +31))/4 , TFT_getfontheight()+6, TFT_WHITE);


	TFT_setclipwin(0,140, _width-1, _height-TFT_getfontheight()-15); // was 160
	xSemaphoreGive(accessTFT);}
}


static void disp_menu2() {

	while (1) {
	struct alarm_data buf_out_alarm_data;
	//struct alarm_data alarms[10];
			if(!xQueueReceive(menu2Queue, (void * ) &buf_out_alarm_data, portMAX_DELAY)) {
			 puts("Display Failed to receive item within 1500 ms");
				        }
				        else {
			// copieer het binnenkomende alarm in de tabel

			 strcpy(alarms[last_alarm].waar, buf_out_alarm_data.waar );
			 strcpy(alarms[last_alarm].wat, buf_out_alarm_data.wat );
			 strcpy(alarms[last_alarm].waarde, buf_out_alarm_data.waarde );
			 alarms[last_alarm].tijdstip = buf_out_alarm_data.tijdstip;

				//if (xSemaphoreTake(accessMenu, 1000)){
					if (xSemaphoreTake(accessTFT , 1000)){
					TFT_saveClipWin();
					TFT_resetclipwin();
					// clear screen
					TFT_fillRect(0, 0, _width-1, 320-30,TFT_BLACK);
					font_transparent =1;
					TFT_setFont(2, NULL); //TFT_setFont(DEFAULT_FONT, NULL);
					char tmp_buff[7];
					_fg = TFT_BLACK;
					TFT_fillRect(10,2+ 1*(TFT_getfontheight()*1.3), _width-12,TFT_getfontheight()+2, TFT_GREEN);
					TFT_print( "Waar        Wat       Status  Wanneer ", 10,5+ 1*(TFT_getfontheight()*1.3)); // (x,y absoluut
					_fg = TFT_WHITE;

				bool busy = true;

				int line = 2;
				int arptr =last_alarm;
				last_alarm++; if (last_alarm ==6) last_alarm = 0;// zeven meldingen in de tabel en zeven plaatsen op het scherm


while (busy){
			//waar
			TFT_drawRect(10,2+ line*(TFT_getfontheight()*1.3), _width-12,TFT_getfontheight()+2, TFT_CYAN);
			TFT_print(alarms[arptr].waar,12, 4+   line*(TFT_getfontheight()*1.3)); // (x,y absoluut
			//wat
			TFT_drawRect(125,2+ line*(TFT_getfontheight()*1.3), _width-12,TFT_getfontheight()+2, TFT_CYAN);
			TFT_print(alarms[arptr].wat,127, 4+   line*(TFT_getfontheight()*1.3)); // (x,y absoluut
			//waarde / status
			TFT_drawRect(230,2+ line*(TFT_getfontheight()*1.3), _width-12,TFT_getfontheight()+2, TFT_CYAN);
			TFT_print(alarms[arptr].waarde,232, 4+   line*(TFT_getfontheight()*1.3)); // (x,y absoluut
			//wanneer
			itoa(alarms[arptr].tijdstip.tm_hour, tmp_buff,10);
			TFT_print(tmp_buff, 360,4+ line*(TFT_getfontheight()*1.3));
			TFT_print(":", LASTX,4+ line*(TFT_getfontheight()*1.3));
			itoa(alarms[arptr].tijdstip.tm_min, tmp_buff,10);
			TFT_print(tmp_buff, LASTX,4+ line*(TFT_getfontheight()*1.3));
			itoa(alarms[arptr].tijdstip.tm_sec, tmp_buff,10);
			TFT_print(":", LASTX,4+ line*(TFT_getfontheight()*1.3));
			TFT_print(tmp_buff, LASTX,4+ line*(TFT_getfontheight()*1.3));
			arptr = arptr -1; if (arptr <0)arptr =5; line++; if(line==8) busy=false;
}


	vTaskDelay(10000 / portTICK_PERIOD_MS);
	TFT_fillRect(0, 0, _width-1, 320-30,TFT_BLACK);
	_fg = TFT_WHITE;
	font_transparent =0;
	//disp_header();
	TFT_restoreClipWin();
	xSemaphoreGive(accessTFT);
	disp_header("");
	//xSemaphoreGive(accessMenu);
	//}
	}
}
	}// while 1
}


static void update_header(char *ftr, int pos, color_t color)
{
	if (xSemaphoreTake(accessTFT, 3000)){
	color_t last_fg, last_bg;

	TFT_saveClipWin();
	TFT_resetclipwin();

	Font curr_font = cfont;
	last_bg = _bg;
	last_fg = _fg;
	_fg = TFT_YELLOW;
	_bg = (color_t){ 64, 64, 64 };
	TFT_setFont(DEFAULT_FONT, NULL);
	font_transparent =1;
	if (pos==0) {
		TFT_fillRect(1, 1, -4+_width, TFT_getfontheight()+6, _bg);
		TFT_print(ftr, CENTER, 4);
	}

	if (pos==1) {
		TFT_setFont(1, NULL);
		TFT_fillRoundRect(1, _height-TFT_getfontheight()-8, _width/3-3, TFT_getfontheight()+6,5, _bg);// (x,y absoluut, w en h relatief tov absoluut)
		if (strlen(ftr) == 0) printf("ipv time etc ");
		else TFT_print(ftr,3 , _height- TFT_getfontheight()+4-(TFT_getfontheight())/2); // (x,y absoluut
	}

	if (pos==2) {
			TFT_setFont(1, NULL);
			TFT_fillRoundRect(_width/3 +10, _height-TFT_getfontheight()-8, _width/3-3-30, TFT_getfontheight()+6,5, _bg);// (x,y absoluut, w en h relatief tov absoluut)
			if (strlen(ftr) == 0) printf("ipv time etc ");
			else {TFT_print(ftr,_width/3 +30 , _height- TFT_getfontheight()+4-(TFT_getfontheight())/2); // (x,y absoluut
			TFT_drawRoundRect(_width/3 +10, _height-TFT_getfontheight()-9, _width/3-31, TFT_getfontheight()+8,5, TFT_CYAN);
			}
	}
	_fg = TFT_BLACK;
	if (pos ==3) {
		TFT_setFont(1, NULL);
		TFT_fillRect( _width/3 +10 + _width/3 -31 + 11 , _height-TFT_getfontheight()-8,( _width-(_width/3 +10 + _width/3 +31))/4 , TFT_getfontheight()+6, color);// (x,y absoluut, w en h relatief tov absoluut)
		if (strlen(ftr) != 0) {
		TFT_print(ftr,_width/3 +10 + _width/3 -31 + 11 +6, _height- TFT_getfontheight()+4-(TFT_getfontheight())/2); // (x,y absoluut
			}

		}

	if ( pos ==4) {
			TFT_setFont(1, NULL);
			TFT_fillRect(_width/3 +10 + _width/3 -31 +  11 + (_width-(_width/3 +10 + _width/3 +31))/4  + 11, _height-TFT_getfontheight()-8, (_width-(_width/3 +10 + _width/3 +31))/4 , TFT_getfontheight()+6, color);
			if (strlen(ftr) != 0) {
			TFT_print(ftr,_width/3 +10 + _width/3 -31 +  11 + (_width-(_width/3 +10 + _width/3 +31))/4  + 11 + 6 , _height- TFT_getfontheight()+4-(TFT_getfontheight())/2); // (x,y absoluut
		}
		}


	if ( pos ==5) {
			TFT_setFont(1, NULL);
			TFT_fillRect(_width/3 +10 + _width/3 -31 +  3*11 + 2*(_width-(_width/3 +10 + _width/3 +31))/4, _height-TFT_getfontheight()-8, (_width-(_width/3 +10 + _width/3 +31))/4 , TFT_getfontheight()+6, color);
			if (strlen(ftr) != 0) {
			TFT_print(ftr,_width/3 +10 + _width/3 -31 +  3*11 + 2*(_width-(_width/3 +10 + _width/3 +31))/4 + 6 , _height- TFT_getfontheight()+4-(TFT_getfontheight())/2); // (x,y absoluut
		}
		}

	if ( pos ==6) {
			TFT_setFont(1, NULL);
			TFT_fillRect(_width/3 +10 + _width/3 -31 +  4*11 + 3*(_width-(_width/3 +10 + _width/3 +31))/4, _height-TFT_getfontheight()-8, (_width-(_width/3 +10 + _width/3 +31))/4 , TFT_getfontheight()+6, color);
			if (strlen(ftr) != 0) {
			TFT_print(ftr,_width/3 +10 + _width/3 -31 +  4*11 + 3*(_width-(_width/3 +10 + _width/3 +31))/4 + 6 , _height- TFT_getfontheight()+4-(TFT_getfontheight())/2); // (x,y absoluut
		}
		}

	font_transparent =0;
	cfont = curr_font;
	_fg = last_fg;
	_bg = last_bg;

	TFT_restoreClipWin();
	xSemaphoreGive(accessTFT);}
}

static void update_venster(int venster, int line,char *text, char *num, const color_t color)
{
	if (xSemaphoreTake(accessTFT, 3000)){
	color_t last_fg, last_bg;

	TFT_saveClipWin();
	TFT_resetclipwin();
// save background and foreground color en font
	Font curr_font = cfont;
	last_bg = _bg;
	last_fg = _fg;

// nieuwe 	background and foreground color en font

	_fg = TFT_YELLOW;
	_bg = (color_t){ 64, 64, 64 };
	//TFT_setFont(DEFAULT_FONT, NULL);
	TFT_setFont(2, NULL);
	switch (venster) {
	case links:{

			//TFT_fillRoundRect(10,2+ line*(TFT_getfontheight()*1.3), _width/2-12,TFT_getfontheight()+2 ,5,_bg);

			TFT_fillRect(10,2+ line*(TFT_getfontheight()*1.3), _width/2-12,TFT_getfontheight()+2,_bg);
			TFT_fillRoundRect(_width/2-110,2+ line*(TFT_getfontheight()*1.3), 107,TFT_getfontheight()+2,5,color);

			//TFT_fillRect(_width/2-110,2+ line*(TFT_getfontheight()*1.3), 107,TFT_getfontheight()+2,color);
			if (line==0) _fg = TFT_WHITE;
			_fg = TFT_WHITE;

			TFT_print(text,12, 4+   line*(TFT_getfontheight()*1.3)); // (x,y absoluut

			_fg = TFT_BLACK;
			font_transparent =1;
			TFT_print(num,_width/2-108, 4+   line*(TFT_getfontheight()*1.3)); // (x,y absoluut
			font_transparent =0;
			_fg = TFT_YELLOW;
			break;
	}


	case rechts:
	{
		 	TFT_fillRect(_width/2+10,2+ line*(TFT_getfontheight()*1.3), _width/2-12,TFT_getfontheight()+2,_bg);
		 	if (line !=0){
		 	TFT_fillRoundRect(_width -100,2+ line*(TFT_getfontheight()*1.3), 95,TFT_getfontheight()+2,5,color);
		 	//TFT_fillRect(_width -100,2+ line*(TFT_getfontheight()*1.3), 95,TFT_getfontheight()+2,color);
		 	}
			//if (line==0) _fg = TFT_WHITE;
		 	_fg = TFT_WHITE;
		 	TFT_print(text,_width/2+12, 5+   line*(TFT_getfontheight()*1.3)); // (x,y absoluut
			//_fg = color;
			_fg = TFT_BLACK;
			font_transparent =1;
			TFT_print(num,_width-100, 5+   line*(TFT_getfontheight()*1.3)); // (x,y absoluut
			font_transparent =0;
			_fg = TFT_YELLOW;
			break;
	}
	}

	// restore background and foreground color en font

	cfont = curr_font;
	_fg = last_fg;
	_bg = last_bg;

	TFT_restoreClipWin();
	xSemaphoreGive(accessTFT);}
}



struct Graph_info {
   int xlo ;
   int xhi;
   int ylo;
   int yhi;
   int xinc;
   int yinc;
   bool init_done;
} Graph1;

static struct Graph_info resultaat;  // vasthouden resultaat voor de volgende aanroep

/* ----zorgt dat de schaal meelopt met de min en max x en y */

struct Graph_info  Graph_scale( int x, int y){
if (!resultaat.init_done) {
resultaat.init_done = true;
resultaat.xlo = x;  // voorkomt dat er 0 blijft staan
resultaat.ylo = y;  // voorkomt dat er 0 blijft staan
resultaat.xhi = 0;  // voorkomt dat max blijft staan
resultaat.yhi = 0;  // voorkomt dat max blijft staan
}

if (x > resultaat.xhi) resultaat.xhi =x;
if (y > resultaat.yhi) resultaat.yhi =y;
if (x < resultaat.xlo) resultaat.xlo =x;
if (y < resultaat.ylo) resultaat.ylo =y;

resultaat.xinc = (resultaat.xhi- resultaat.xlo)/5; if (resultaat.xinc ==0) resultaat.xinc=1;
resultaat.yinc = (resultaat.yhi- resultaat.ylo)/5; if (resultaat.yinc ==0) resultaat.yinc=1;
return (resultaat);
};


/* ==== zorgt dat de schaal weer opnieuw ingesteld kan worden  voor een nieuwe grafiek --*/
void  Graph_reset( ){

	resultaat.init_done = false;
}

/*

  function to draw a cartesian coordinate system and plot whatever data you want
  just pass x and y and the graph will be drawn

  x = x data point
  y = y datapont
  gx = x graph location (lower left)
  gy = y graph location (lower left)
  w = width of graph
  h = height of graph
  xlo = lower bound of x axis
  xhi = upper bound of x asis
  xinc = division of x axis (distance not count)
  ylo = lower bound of y axis
  yhi = upper bound of y asis
  yinc = division of y axis (distance not count)
  title = title of graph
  xlabel = x asis label
  ylabel = y asis label
  gcolor = graph line colors
  acolor = axi ine colors
  pcolor = color of your plotted data
  tcolor = text color
  bcolor = background color
  &redraw = flag to redraw graph on fist call only
*/


/*   tekent de grafiek ---*/
static void Graph( int x, int y, int gx, int gy, int w, int h, int xlo, int xhi, int xinc, int ylo, int yhi, int yinc, char  *title, char *xlabel, char  *ylabel,  color_t gcolor,  color_t acolor,  color_t pcolor,  color_t tcolor,  color_t bcolor, bool redraw) {
  char buf [3];
  int ydiv, xdiv;
  int i;
  int temp;
  int rot, newrot;
 static  int ox, oy;
if (((xhi - xlo) + gx)!=0 && ((yhi - ylo) + gy) !=0) {
  if (redraw == true) {

    redraw = false;
// let op xo en yo staan in een static variabele
   ox = (x - xlo) * ( w) / (xhi - xlo) + gx;
   oy = (y - ylo) * (-h) / (yhi - ylo) + gy;
    // Y as
    for ( i = ylo; i <= yhi; i += yinc) {
      // de transformatie
      temp =  (i - ylo) * (-h) / (yhi - ylo) + gy;

      if (i == 0) {
        TFT_drawLine(gx, temp, gx + w, temp, gcolor);
      }
      else {
        TFT_drawLine(gx, temp, gx + w, temp, gcolor);
      }
      // draw the axis labels
      TFT_setFont(SMALL_FONT, NULL);
      _fg = bcolor;
      font_transparent =1;
      itoa(i,buf,10);
      TFT_print(buf, gx-20,temp); // was -40
    }
    // x as
    for (i = xlo; i <= xhi; i += xinc) {

      // de transformatie
      temp =  (i - xlo) * ( w) / (xhi - xlo) + gx;
      if (i == 0) {
        TFT_drawLine(temp,gy,temp ,gy - h, gcolor);
      }
      else {
        TFT_drawLine(temp,gy,temp ,gy - h, gcolor);
      }
      // as labels
      TFT_setFont(SMALL_FONT, NULL);
      _fg = bcolor;
      font_transparent =1;
      itoa(i,buf,10);
      TFT_print( buf,temp, gy + 10);
    }
    TFT_setFont(SMALL_FONT, NULL);
    _fg = bcolor;
    font_transparent =1;
    TFT_print(title,gx, gy -h- 30);  // was - 30
    TFT_setFont(SMALL_FONT, NULL);
    _fg = bcolor;
    font_transparent =1;
    TFT_print(xlabel, gx, gy + 20);
    TFT_setFont(SMALL_FONT, NULL);
    _fg = bcolor;
    font_transparent =1;
    TFT_print(ylabel,gx - 30, gy -h - 10);
  }

  // plot de data, maak de punten ook wat dikker

  x =  (x - xlo) * ( w) / (xhi - xlo) + gx;
  y =  (y - ylo) * (-h ) / (yhi - ylo) + gy;
  TFT_drawLine(ox, oy,x,y, pcolor);
  TFT_drawLine(ox, oy+1,x,y+1, pcolor);
  TFT_drawLine(ox, oy-1,x,y-1, pcolor);
  ox = x;
  oy = y;
}
}







