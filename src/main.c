/*
 * Example for using I2C with 128x32 graphic OLED
 * 03-29-2023 E. Brombaugh
 */

// what type of OLED - uncomment just one
//#define SSD1306_64X32
#define SSD1306_128X32
//#define SSD1306_128X64

#include "ch32v003fun.h"
#include <stdio.h>
#include "ssd1306_i2c.h"
#include "ssd1306.h"

#include "i2c.h"
#include "bmp280.h"
#include "aht20.h"


uint8_t battery_level = 40;
uint8_t bmp280_errno = 0;
uint8_t aht20_errno = 0 ;
BMP280Data bmp280_data;
AHT20Data aht20_data;

char buf[12];

void drawClock() {
	ssd1306_drawRect(0, 0,128, 32,0);
	ssd1306_fillRect(58, 0, 20, 3, 1);
	ssd1306_fillRect(58, 29, 20, 3, 1);
	ssd1306_drawLine(58, 3, 77, 29, 1);
	ssd1306_drawLine(77, 3, 58, 29, 1);
	ssd1306_refresh();
}

void drawAll() {

	if (!aht20_errno) {
		sprintf(buf, "%d", abs((int16_t)aht20_data.temperature));
		ssd1306_setbuf(0);
		if(aht20_data.temperature < 0) {
			ssd1306_fillRect(0, 15, 7, 3, 1);
		}
		ssd1306_drawstr_sz(8, 0, buf,1, fontsize_32x32);
		sprintf(buf, "%5d%%", (uint8_t)aht20_data.humidity);
		ssd1306_drawstr_sz(80, 24, buf,1, fontsize_8x8);
	}

	else{
		ssd1306_drawstr_sz(0, 0, "AHT20 failure!",1, fontsize_8x8);
	}

	if(!bmp280_errno) {
		sprintf(buf, "%6d mm", (uint32_t)((float)(bmp280_data.pressure) / 133.322f));
		printf(buf);
		printf("PRESS DRAW: %d", bmp280_data.pressure);
		ssd1306_drawstr_sz(80, 12, buf,1, fontsize_8x8);
	}
	else {
		ssd1306_drawstr_sz(80, 12, "ERROR",1, fontsize_8x8);
	}

	ssd1306_fillRect(100, 0, 24, 8, 1);
	ssd1306_fillRect(124, 2, 2, 4, 1);
	ssd1306_fillRect(101, 1, 22, 6, 0);
	ssd1306_fillRect(102, 2, battery_level / 5, 4, 1);

	ssd1306_refresh();
}

int main()
{
	// 48MHz internal clock
	//RCC->CFGR0 |= RCC_HPRE_DIV2;
	SystemInit();

	RCC->CFGR0 |= RCC_HPRE_DIV4;
	
	printf("Startup...\n");
    if(i2c_init() != I2C_OK)
		printf("Failed to init the I2C Bus\n");

	Delay_Ms(5);

	drawClock();

	Delay_Ms(100);
	bmp280_errno = BMP280_read(&bmp280_data);
	aht20_errno = AHT20_read(&aht20_data);

	ssd1306_init();
	drawAll();
	
	printf("Stuck here forever...\n\r");
	while(1);
}