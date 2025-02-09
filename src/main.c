
#define SSD1306_128X32


#include "ch32v003fun.h"
#include <stdio.h>
#include "ssd1306_i2c.h"
#include "ssd1306.h"

#include "i2c.h"
#include "bmp280.h"
#include "aht20.h"
#include "uart.h"

#define millis() (SysTick->CNT / DELAY_MS_TIME)


#define SLEEP_AFTER 3000 // Timeout before dimm screen

#define SCREEN_POWER_PIN PC6
#define SENSOR_POWER_PIN PC7

// Preshared key for BLE auth
#define AUTH_PSK "daiPoocaesheeQuie3ti"

uint8_t battery_level = 40;
uint8_t bmp280_errno = 0;
uint8_t aht20_errno = 0 ;
BMP280Data bmp280_data;
AHT20Data aht20_data;

char buf[256] = {0};



void drawClock() {
	ssd1306_drawRect(0, 0,128, 32,0);
	ssd1306_fillRect(58, 0, 20, 3, 1);
	ssd1306_fillRect(58, 29, 20, 3, 1);
	ssd1306_drawLine(58, 3, 77, 29, 1);
	ssd1306_drawLine(77, 3, 58, 29, 1);
	ssd1306_refresh();
}

void drawAll() {
	ssd1306_setbuf(0);
	if (!aht20_errno) {
		sprintf(buf, "%d", abs((int16_t)aht20_data.temperature));
		
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

volatile int num = 0;

uint8_t authorized = 0;

#define CMD_AUTH "AT+AUTH"
#define CMD_DATA "AT+DATA"

uint8_t process_command(char *command) {
	if((strncmp(command, CMD_AUTH, strlen(CMD_AUTH)) == 0) &&
		strcmp((char*)(command + strlen(CMD_AUTH)), AUTH_PSK) == 0) {
		
		uart_print("+Authorized\r\n");
		authorized = 1;
		command_ack();
		return 0;
	}
	if (authorized && (strcmp(CMD_DATA, command) == 0)) {

		sprintf(buf, "{\"t\":\"%d\", \"h\":\"%d\",\"p\":\"%d\"}", 
			aht20_data.temperature, aht20_data.humidity, bmp280_data.pressure);
		uart_print(buf);
		uart_print("\r\n");
		command_ack();
		return 0;
	}
// AT+AUTHdaiPoocaesheeQuie3ti
	uart_print("+UnknownCmdOrUnauthorized\r\n");	
	command_ack();
	return 1;
}


void init() {
	
	// ADC config
	funAnalogInit();

	// GPIO config
	funGpioInitAll();
	funPinMode(SCREEN_POWER_PIN, GPIO_Speed_10MHz | GPIO_CNF_OUT_OD);
	funPinMode(SENSOR_POWER_PIN, GPIO_Speed_10MHz | GPIO_CNF_OUT_OD);
	
	// Deep Sleep config
	PWR->CTLR |= PWR_CTLR_PDDS;
	PFIC->SCTLR |= 1 << 2;

	// Run with 6 Mhz system core clock
	RCC->CFGR0 |= RCC_HPRE_DIV4;

	// Reset SysTick
	SysTick->CNT = 0;
}


void deepsleep() {
	__WFE();
}

int main()
{
	// 48MHz internal clock
	SystemInit();
	init();


	uart_init();

	uart_print("+MCUREADY\r\n");
	

	buf[0] = 'A';

	// Sink power for screen and sensor GND
	funDigitalWrite(SCREEN_POWER_PIN, 0);
	funDigitalWrite(SENSOR_POWER_PIN, 0);
	

	printf("Startup...\n");
    if(i2c_init() != I2C_OK)
		printf("Failed to init the I2C Bus\n");



	// Give screen  to warmup from cold start
	Delay_Ms(50);

	// Draw sandclock until no results are fetched
	ssd1306_init(); // init twice - dirty hack to avoid problem wen display is not starting after ground loss
	
	drawClock();

	// for(int i = 0; i< 5; i++) {
	// 	funDigitalWrite(SCREEN_POWER_PIN, 0);
	// 	Delay_Ms(1000);
	// 	funDigitalWrite(SCREEN_POWER_PIN, 1);
	// 	Delay_Ms(1000);
	// }
	


	// Sensors requires 80ms+ to start
	Delay_Ms(100);
	bmp280_errno = BMP280_read(&bmp280_data);
	Delay_Ms(5);
	aht20_errno = AHT20_read(&aht20_data);

	//Power down sensor
	funDigitalWrite(SENSOR_POWER_PIN, 1);

	//funDigitalWrite(SCREEN_POWER_PIN, 0);

	drawAll();

	// ssd1306_setbuf(0);
	// ssd1306_refresh();

	
	while(1) {
		//volatile b = USART1->DATAR;
		//volatile c = abs((uint8_t);

		//buf[0] = USART1->DATAR;
		//printf("statr: 0x%x, num %d, IEPR 0x%x buf: %s\n", USART1->STATR, num, PFIC->IPR, buf);

		if(is_command_received()) {
			printf("Command: %s\n", get_command());
			process_command(get_command());
		}

		//printf("cr: %d cmd %s\n", command_received, command);
		Delay_Ms(100);

		// ssd1306_drawstr(0,0,buf,1);
		// ssd1306_refresh();

		// if ((millis() % 2000 ) > 1000) {
		// 	funDigitalWrite(SCREEN_POWER_PIN, 1);
		// }
		// else {
		// 	funDigitalWrite(SCREEN_POWER_PIN, 0);
		// }

		if(millis() > 3000) {
			// Shutdown screen

			//uint8_t command = 0xAE; //display off
			


			// uint8_t power_save_cmds[] = {
			// 	0xAE,  // Display off (no image)
			// 	0x8D,  // Set Charge Pump
			// 	0x10,  // Disable Charge Pump (optional, saves power)
			// 	0xA4   // Display Normal (reset any previous modes)
			// };
			// i2c_begin_transmisison(SSD1306_I2C_ADDR, I2C_MODE_WRITE);
			// i2c_transmit_data(power_save_cmds, sizeof(power_save_cmds));
			// i2c_end_transmisison();
			// i2c_deinit();


			funDigitalWrite(SCREEN_POWER_PIN, 1);
		 	//__WFI();
		}
	}
}