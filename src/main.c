
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


#define SCREEN_DELAY 3000 // Timeout before dimm screen
#define COMM_DELAY 10000	//Max time for UART communitation before sleep

#define SCREEN_GND_PIN PC6
#define SENSOR_GND_PIN PC7
#define I2C_VCC_PIN PC4
#define WAKEUP_PIN PA1
#define BAT_ADC_PIN ANALOG_0

// Preshared key for BLE auth
#define AUTH_PSK "123"

uint8_t battery_level = 0;
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
#define CMD_JDY23_READY "+Ready" //JDY-23 is booted

// Function to convert battery voltage in millivolts (vbat_mv) to battery percentage
int vbat_to_percentage(int vbat_mv) {
    // Clamping the values to ensure they're within the valid range
    if (vbat_mv <= 1500) {
        return 0;  // 0% when voltage is below 1.5V
    }
    if (vbat_mv >= 2100) {
        return 100;  // 100% when voltage is above 2.1V
    }

    // Linear mapping between 1.5V (0%) and 2.1V (100%)
    return (vbat_mv - 1500) * 100 / (2100 - 1500);
}

uint8_t measure() {
	funDigitalWrite(SENSOR_GND_PIN, 0);
	// Sensors requires 80ms+ to start
	Delay_Ms(100);
	uint8_t bmp280_errno = BMP280_read(&bmp280_data);
	Delay_Ms(5); // We need it, for some reason BMP not worjing without small delay
	uint8_t aht20_errno = AHT20_read(&aht20_data);
	funDigitalWrite(SENSOR_GND_PIN, 1);
	
	funPinMode(PA2, GPIO_CNF_IN_ANALOG);
	volatile uint16_t vref = funAnalogRead(ANALOG_8);
    volatile uint16_t vbat = funAnalogRead(BAT_ADC_PIN);
    volatile uint16_t vbat_mv = (1200 *vbat) / vref; // Battery voltage in milivolts
	funPinMode(PA2, GPIO_CNF_IN_PUPD);
	battery_level = vbat_to_percentage(vbat_mv);

	return bmp280_errno | aht20_errno;
}

uint8_t process_command(char *command) {

	if(strcmp(command, CMD_JDY23_READY) == 0) {
		// init JDY-23
		printf("JDY23 ready cmd\\r\n");
	}

	if((strncmp(command, CMD_AUTH, strlen(CMD_AUTH)) == 0) &&
		strcmp((char*)(command + strlen(CMD_AUTH)), AUTH_PSK) == 0) {
		
		uart_print("+Authorized\r\n");
		authorized = 1;
		command_ack();
		return 0;
	}
	if (authorized && (strcmp(CMD_DATA, command) == 0)) {

		if(measure() == BMP280_OK) {
			sprintf(buf, "{\"t\":\"%d\",\"h\":\"%d\",\"p\":\"%d\",\"b\":\"%d\"}", 
				(uint16_t)aht20_data.temperature, 
				(uint16_t)aht20_data.humidity, 
				(uint32_t)((float)(bmp280_data.pressure) / 133.322f),
				battery_level+1);
			
			uart_print(buf);
			uart_print("\r\n");
			printf("%s\r\n", buf);
			command_ack();
			return 0;
		}
		else
		{
			uart_print("{\"err\":\"sensor failure\"}\r\n");
			command_ack();
			return 0;
		}
	}

	uart_print("---");
	uart_print(command);
	uart_print("+UnknownCmdOrUnauthorized\r\n");	
	command_ack();
	return 1;
}

void EXTI7_0_IRQHandler( void ) __attribute__((interrupt));
void EXTI7_0_IRQHandler( void ) 
{
    if ((EXTI->INTFR & EXTI_Line1)) {
        EXTI->INTFR |= EXTI_Line1;
    }
}

void init() {
	
	// ADC config
	funAnalogInit();

	// GPIO config
	funGpioInitAll();

	//GPIOA: Set to output
	GPIOA->CFGLR = (GPIO_CNF_IN_PUPD<<(4*2)) |
				   (GPIO_CNF_IN_PUPD<<(4*1));
	GPIOA->BSHR = GPIO_BSHR_BS2 | GPIO_BSHR_BR1;
	// GPIOC: Set to input with mixed pull-up / pull-down
	GPIOC->CFGLR = (GPIO_CNF_IN_PUPD<<(4*7)) |
				   (GPIO_CNF_IN_PUPD<<(4*6)) |
				   (GPIO_CNF_IN_PUPD<<(4*5)) |
				   (GPIO_CNF_IN_PUPD<<(4*4)) |
				   (GPIO_CNF_IN_PUPD<<(4*3)) |
				   (GPIO_CNF_IN_PUPD<<(4*2)) |
				   (GPIO_CNF_IN_PUPD<<(4*1)) |
				   (GPIO_CNF_IN_PUPD<<(4*0));
	GPIOC->BSHR = GPIO_BSHR_BS7 |
				  GPIO_BSHR_BR6 |
				  GPIO_BSHR_BS5 |
				  GPIO_BSHR_BR4 |
				  GPIO_BSHR_BS3 |
				  GPIO_BSHR_BR2 |
				  GPIO_BSHR_BS1 |
				  GPIO_BSHR_BR0;
	// GPIOD: D2 set to input pull-up
	GPIOD->CFGLR = (GPIO_CNF_IN_PUPD<<(4*7)) |
				   (GPIO_CNF_IN_PUPD<<(4*6)) |
				   (GPIO_CNF_IN_PUPD<<(4*5)) |
				   (GPIO_CNF_IN_PUPD<<(4*4)) |
				   (GPIO_CNF_IN_PUPD<<(4*3)) |
				   (GPIO_CNF_IN_PUPD<<(4*2)) |
				   (GPIO_CNF_IN_FLOATING<<(4*1)) |
				   (GPIO_CNF_IN_PUPD<<(4*0));
	// GPIOD->BSHR = GPIO_BSHR_BR7 |
	// 			  GPIO_BSHR_BS6 |
	// 			  GPIO_BSHR_BR5 |
	// 			  GPIO_BSHR_BS4 |
	// 			  GPIO_BSHR_BR3 |
	// 			  GPIO_BSHR_BS2 |
	// 			  GPIO_BSHR_BR0;


	funPinMode(SCREEN_GND_PIN, GPIO_Speed_2MHz | GPIO_CNF_OUT_OD);
	funPinMode(SENSOR_GND_PIN, GPIO_Speed_2MHz | GPIO_CNF_OUT_OD);
	funPinMode(I2C_VCC_PIN, GPIO_Speed_2MHz | GPIO_CNF_OUT_PP);
	funPinMode(WAKEUP_PIN, GPIO_CFGLR_IN_PUPD);
	
	// Supply i2c power
	funDigitalWrite(I2C_VCC_PIN, 1);

	// PA1 interupt wakeup
	AFIO->EXTICR |= AFIO_EXTICR_EXTI1_PA;
	EXTI->INTENR |= EXTI_INTENR_MR1; // Enable EXT1
	EXTI->RTENR |= EXTI_RTENR_TR1;
	RCC->APB2PCENR |= RCC_AFIOEN;
    NVIC_EnableIRQ( EXTI7_0_IRQn );
	
	// Deep Sleep config
	PWR->CTLR |= PWR_CTLR_PDDS;
	PFIC->SCTLR |= 1 << 2;

	// Run with 6 Mhz system core clock
	RCC->CFGR0 |= RCC_HPRE_DIV4;

	uart_init();
	
    if(i2c_init() != I2C_OK) {
		printf("Failed to init the I2C Bus\n");
		uart_print("+I2CFAILURE\r\n");
	}
	// Reset SysTick
	SysTick->CNT = 0;

	Delay_Ms(50);
	ssd1306_init();

	authorized = 0;
}

void deinit() {
	i2c_deinit();
	uart_deinit();

    //Disable and reset ADC
    RCC->APB2PCENR &= ~RCC_APB2Periph_ADC1;
    RCC->APB2PRSTR |= RCC_APB2Periph_ADC1;

	funDigitalWrite(I2C_VCC_PIN, 0);
	funPinMode(SCREEN_GND_PIN, GPIO_Speed_2MHz | GPIO_CNF_OUT_PP);
	funPinMode(SENSOR_GND_PIN, GPIO_Speed_2MHz | GPIO_CNF_OUT_PP);

	//power off SCL
	// funPinMode(PC1, GPIO_CNF_OUT_OD);
	// funPinMode(PC1, GPIO_CNF_OUT_OD);


	funPinMode(PD1, GPIO_CNF_IN_PUPD); // disable floating in SWIO pin for power eficiency
}



void deepsleep() {
	uart_print("+SLEEP\r\n");
	deinit();
	Delay_Ms(1); // Give a chance to deliver DATA byte
	__WFI();
	SystemInit();
	init();
}

int main()
{
	// 48MHz internal clock
	SystemInit();
	init();

	uart_print("+MCUREADY\r\n");
	
	// Sink power for screen and sensor GND
	funDigitalWrite(SCREEN_GND_PIN, 0);

	// Give screen  to warmup from cold start
	Delay_Ms(100);

	// Draw sandclock until no results are fetched
	//ssd1306_init(); // init twice - dirty hack to avoid problem wen display is not starting after ground loss
	drawClock();

	measure();
	drawAll();
	
	while(millis() <= SCREEN_DELAY) {
		if(is_command_received()) {
			printf("Command: %s\n", get_command());
			process_command(get_command());
		}
	}

	ssd1306_setbuf(0);
	ssd1306_refresh();
	funDigitalWrite(SCREEN_GND_PIN, 1);

	uart_print("+SLEEP\r\n");
	deepsleep();

	while(1) {
		if(is_command_received()) {
			printf("Command: %s\n", get_command());
			process_command(get_command());
		}
		if(millis() > COMM_DELAY) {
			
			deepsleep();
			uart_print("+WAKEUP\r\n");
		}
	}
}

/*
AT+AUTH123
AT+DATA
 */