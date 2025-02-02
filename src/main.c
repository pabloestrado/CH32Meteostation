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
//#include "lib_i2c.h"


#define AHT20_ADDR 0x38    // AHT20 I2C address (7-bit)
#define CMD_INIT 0x00     // Initialization command
#define CMD_MEASURE 0xAC  // Measure command


uint8_t battery_level = 40;

char buf[12];

void i2c_callback(uint8_t addr) {
    printf("Found i2c device %x \n", addr);
}

// I2C defaults
#define I2C_CLK_RATE	500000	// I2C clock frequency 50KHz
#define I2C_AFIO_REG	((uint32_t)0x00000000)
#define I2C_PORT_RCC	RCC_APB2Periph_GPIOC
#define I2C_PORT		GPIOC
#define I2C_PIN_SCL 	2
#define I2C_PIN_SDA 	1
#define I2C_PRERATE 1000000

// I2C predefines
#define I2C_MODE_READ 	0
#define I2C_MODE_WRITE 	1
#define I2C_OK 0
#define I2C_ERR_BUSY	1 // I2C bus is busy
#define I2C_ERR_MASTER  2 // Error starting master mode on I2C bus
#define I2C_ERR_ADDR    3 // Error setting address
#define I2C_ERR_TIMEOUT 4 // I2C timeout
#define I2C_ERR_BERR	5 // I2C timeout
#define I2C_TIMEOUT 	3000 
//#define I2C_DEBUG 		1 // echo all I2C data and errors to printf

#define AHT20_I2CADDR_DEFAULT 	0x38 // AHT default i2c address
#define AHT20_CMD_CALIBRATE 	0xBE // Calibration command
#define AHT20_CMD_TRIGGER 		0xAC // Trigger reading command
#define AHT20_CMD_SOFTRESET 	0xBA // Soft reset command
#define AHT20_STATUS_BUSY 		0x80 // Status bit for busy
#define AHT20_STATUS_CALIBRATED 0x08 // Status bit for calibrated

#define AHT20_OK				0x00 // No error
#define AHT20_ERR_NOTFOUND 		0x01 // Error: sensor not responding on I2C bus		
#define AHT20_ERR_CALIBRATION 	0x02 // Calibration error
#define AHT20_ERR_MEASUREMENT 	0x03 // Measurement error


#define BMP280_OK				0x00 // No error
#define BMP280_ERR_NOTFOUND 	0x01 // Error: sensor not responding on I2C bus		
#define BMP280_ERR_MEASUREMENT 	0x03 // Measurement error



// Begin transmission
// addr = I2C address
// transmit = direction of operation
// transmit = 0: read data from slave
// transmit = 1: write data to slave

uint8_t i2c_begin_transmisison(uint8_t addr, uint8_t transmit) {
	// Master mode
	int32_t timeout = I2C_TIMEOUT;
	while(I2C1->STAR2 & I2C_STAR2_BUSY) { 
		if(--timeout < 0) {
			#ifdef I2C_DEBUG
			printf("I2C Timeout waiting bus\n");
			#endif
			return I2C_ERR_BUSY;
		}
	}

	// Clear Acknowledge failure bit
	I2C1->STAR1 &= !I2C_STAR1_AF; 

	I2C1->CTLR1 |= I2C_CTLR1_START;
	timeout = I2C_TIMEOUT;
	// Assert I2C master mode
	while((I2C1->STAR1 != 0x0001) || (I2C1->STAR2 != 0x0003)) // BUSY, MSL, SB flags
	if(--timeout < 0) {
		#ifdef I2C_DEBUG
		printf("I2C start timeout\n");
		#endif
		return I2C_ERR_MASTER;
	}

	I2C1->DATAR = (addr << 1) & 0xFE;

	//Assert transmitter master mode
	timeout = I2C_TIMEOUT;
	while((I2C1->STAR1 != 0x0082) || (I2C1->STAR2 != 0x0007)) // BUSY, MSL, SB flags
	if(--timeout < 0) {
		#ifdef I2C_DEBUG
		printf("I2C transmit init timeout\n");
		#endif
		return I2C_ERR_TIMEOUT;
	}
	
	if(transmit == I2C_MODE_READ) {
		I2C1->CTLR1 |= I2C_CTLR1_ACK;

		// Start in transmitting mode
		I2C1->CTLR1 |= I2C_CTLR1_START;
		timeout = I2C_TIMEOUT;
		// Assert I2C master mode
		while((I2C1->STAR1 != 0x0001) || (I2C1->STAR2 != 0x0003)) // BUSY, MSL, SB flags
		if(--timeout < 0) {
			#ifdef I2C_DEBUG
			printf("I2C start in receiver mode timeout\n");
			#endif
			return I2C_ERR_TIMEOUT;
		}

		I2C1->DATAR = (addr << 1) | 0x01;
		//Assert receiver master mode
		timeout = I2C_TIMEOUT;
		while((I2C1->STAR1 != 0x0002) || (I2C1->STAR2 != 0x0003)) // BUSY, MSL, SB flags
		if(--timeout < 0) {
			#ifdef I2C_DEBUG
			printf("Receive init timeout\n");
			#endif
			return I2C_ERR_TIMEOUT;
		}
	}
	return I2C_OK;
}

void i2c_end_transmisison() {
	I2C1->CTLR1 |= I2C_CTLR1_STOP;
	// Clear Acknowledge failure bit, for future operations
	I2C1->STAR1 &= !I2C_STAR1_AF;
	I2C1->STAR1 &= !I2C_STAR1_ARLO; 
}

uint8_t i2c_transmit_data(uint8_t *data, uint16_t length) {
	
	int16_t timeout = I2C_TIMEOUT;

	for (uint16_t cnt = 0; cnt < length; cnt++)
	{
		I2C1->DATAR = *data;
		while(!(I2C1->STAR1 & I2C_STAR1_TXE))
		if(--timeout < 0) {
			#ifdef I2C_DEBUG
			printf("Send  timeout\n");
			#endif
			return I2C_ERR_TIMEOUT;
		}
		#ifdef I2C_DEBUG
		printf("Byte sent: 0x%02X\n", *data);
		#endif
		data++;
	}

	return I2C_OK;
}

uint8_t i2c_receive_data(uint8_t *data, uint16_t length) {

	uint16_t timeout = I2C_TIMEOUT;

	for (uint16_t cnt = 0; cnt < length; cnt++)
	{
		// Set NACK before receiving of the last byte to inform slave to stop transition
		if (cnt == (length - 1)) {
			// Set NACK
			I2C1->CTLR1 &= ~I2C_CTLR1_ACK;
		} 
		else {
			// Set ACK
			I2C1->CTLR1 |= I2C_CTLR1_ACK;
		}

		while(!(I2C1->STAR1 & I2C_STAR1_RXNE))
		if(--timeout < 0) {
			#ifdef I2C_DEBUG
			printf("Receive byte timeout\n");
			#endif
			return I2C_ERR_TIMEOUT;
		}
		*data = I2C1->DATAR;
		#ifdef I2C_DEBUG
		printf("I2C data received: 0x%02x\n", *data);
		#endif
		data++;
	}

	return 0;
}

uint8_t i2c_init()
{
	// Toggle the I2C Reset bit to init Registers
	RCC->APB1PRSTR |=  RCC_APB1Periph_I2C1;
	RCC->APB1PRSTR &= ~RCC_APB1Periph_I2C1;

	// Enable the I2C Peripheral Clock
	RCC->APB1PCENR |= RCC_APB1Periph_I2C1;

	// Enable the selected I2C Port, and the Alternate Function enable bit
	RCC->APB2PCENR |= I2C_PORT_RCC | RCC_APB2Periph_AFIO;

	// Reset the AFIO_PCFR1 register, then set it up
	AFIO->PCFR1 &= ~(0x04400002);
	AFIO->PCFR1 |= I2C_AFIO_REG;

	// Clear, then set the GPIO Settings for SCL and SDA, on the selected port
	I2C_PORT->CFGLR &= ~(0x0F << (4 * I2C_PIN_SDA));
	I2C_PORT->CFGLR |= (GPIO_Speed_10MHz | GPIO_CNF_OUT_OD_AF) << (4 * I2C_PIN_SDA);	
	I2C_PORT->CFGLR &= ~(0x0F << (4 * I2C_PIN_SCL));
	I2C_PORT->CFGLR |= (GPIO_Speed_10MHz | GPIO_CNF_OUT_OD_AF) << (4 * I2C_PIN_SCL);

	// Set the Prerate frequency
	uint16_t i2c_conf = I2C1->CTLR2 & ~I2C_CTLR2_FREQ;
	i2c_conf |= (FUNCONF_SYSTEM_CORE_CLOCK / I2C_PRERATE) & I2C_CTLR2_FREQ;
	I2C1->CTLR2 = i2c_conf;

	// Set I2C Clock
	if(I2C_CLK_RATE <= 100000)
	{
		i2c_conf = (FUNCONF_SYSTEM_CORE_CLOCK / (2 * I2C_CLK_RATE)) & I2C_CKCFGR_CCR;
	} else {
		// Fast mode. Default to 33% Duty Cycle
		i2c_conf = (FUNCONF_SYSTEM_CORE_CLOCK / (3 * I2C_CLK_RATE)) & I2C_CKCFGR_CCR;
		i2c_conf |= I2C_CKCFGR_FS;
	}
	I2C1->CKCFGR = i2c_conf;

	// Enable the I2C Peripheral
	I2C1->CTLR1 |= I2C_CTLR1_PE;

	//TODO:
	// Check error states
	if(I2C1->STAR1 & I2C_STAR1_BERR) 
	{
		I2C1->STAR1 &= ~(I2C_STAR1_BERR); 
		return I2C_ERR_BERR;
	}

	return I2C_OK;
}

typedef struct {
	float temperature;
	float humidity;
} AHT20Data;

AHT20Data sensor_data;
uint8_t aht20_errno = 0 ;

uint8_t AHT20_read(AHT20Data *data) {
	
	uint8_t buf[8] = {0};
	Delay_Ms(200);

	// Calibrate 
	buf[0] = AHT20_CMD_CALIBRATE;
	buf[1] = 0x08;
	buf[2] = 0x00;

	if(i2c_begin_transmisison(AHT20_ADDR, I2C_MODE_WRITE) != 0) {
		// Sensor not responding
		printf("AHT20 sensor or bus is not responding\n");
		i2c_end_transmisison(); //release I2C bus for others
		return AHT20_ERR_NOTFOUND;
	}
	i2c_transmit_data(buf, 3);
	i2c_end_transmisison();
	
	Delay_Ms(10); //Give int some time for calibration
	
	// Read calibration status and check calibrated bit 
	i2c_begin_transmisison(AHT20_ADDR, I2C_MODE_READ);
	i2c_receive_data(buf, 1);
	if(!(buf[0] & AHT20_STATUS_CALIBRATED)) {
		i2c_end_transmisison();
		printf("Calibration failed\n");
		return AHT20_ERR_CALIBRATION; 
	}
	i2c_end_transmisison();

	// Measurement request
	buf[0] = AHT20_CMD_TRIGGER;
	buf[1] = 0x33;
	buf[2] = 0x00; 
	i2c_begin_transmisison(0x38, I2C_MODE_WRITE);
	i2c_transmit_data(buf, 3);
	i2c_end_transmisison();

	Delay_Ms(100); 	// Minimal delay befor reading is 80ms according to the 
					// AHT20 datasheet 100 ms is a good value

	// reading measurement results
	i2c_begin_transmisison(0x38, I2C_MODE_READ);
	// Receive all data from AHT20
	i2c_receive_data(buf, 7);
	if ((buf[0] & 0x80)) {
		i2c_end_transmisison();
		printf("Measurement error\n");
		return AHT20_ERR_MEASUREMENT; // Measurement failed
	}
	i2c_end_transmisison();

	// TODO: add CRC check

	// Combine 20-bit temperature value from result
	uint32_t raw_data;
    raw_data = (((uint32_t)buf[3]) << 16) |
                           (((uint32_t)buf[4]) << 8) |
                           (((uint32_t)buf[5]) << 0);
	
	raw_data = raw_data & 0x000FFFFF; // Keep ony 20 bits of data, zero the rest of them
	float temperature = (float)(raw_data) 
                                 / 1048576.0f * 200.0f
                                 - 50.0f;
	
	// Combine 20-bit humidity value
	raw_data = 0x00000000;
	raw_data = buf[1] << 12;
	raw_data |= buf[2] << 4;
	raw_data |= (buf[3] & 0xF0) >> 4;
	float humidity = ((float)raw_data / 1048576.0f) * 100.0f;

	data->temperature = temperature;
	data->humidity = humidity;
	return AHT20_OK;
}


uint8_t bmp280_errno = 0;

#define BMP280_ADDR 0x77

typedef int32_t BMP280_S32_t;
typedef uint32_t BMP280_U32_t;
typedef int64_t BMP280_S64_t;

typedef struct {
	uint16_t dig_T1;
	int16_t dig_T2;
	int16_t dig_T3;
	uint16_t dig_P1;
	int16_t dig_P2;
	int16_t dig_P3;
	int16_t dig_P4;
	int16_t dig_P5;
	int16_t dig_P6;
	int16_t dig_P7;
	int16_t dig_P8;
	int16_t dig_P9;

}BMP280CalibrationData;


typedef struct {
	int32_t temperature; // in Santi degrees, 2234 means 22.34C
	uint32_t pressure; // in Pa
}BMP280Data;

BMP280Data bmp280_data;

// Returns temperature in DegC, resolution is 0.01 DegC. Output value of “5123”equals 51.23 DegC. 
// t_fine carries fine temperature as global value
BMP280_S32_t t_fine;
BMP280_S32_t BMP280_bmp280_compensate_T_int32(BMP280_S32_t adc_T, BMP280CalibrationData *cd) {
    BMP280_S32_t var1, var2, T;  // Temporary variables for intermediate calculations

    // Calculate the first part of the temperature compensation formula (var1)
    var1 = ((((adc_T >> 3) - ((BMP280_S32_t)cd->dig_T1 << 1))) * ((BMP280_S32_t)cd->dig_T2)) >> 11;

    // Calculate the second part of the temperature compensation formula (var2)
    var2 = (((((adc_T >> 4) - ((BMP280_S32_t)cd->dig_T1)) * ((adc_T >> 4) - ((BMP280_S32_t)cd->dig_T1))) >> 12) * ((BMP280_S32_t)cd->dig_T3)) >> 14;

    // Calculate the fine temperature value (t_fine)
    t_fine = var1 + var2;

    // Calculate the final temperature (T) in degrees Celsius
    T = (t_fine * 5 + 128) >> 8;

    // Return the compensated temperature value
    return T;
}

// Returns pressure in Pa as unsigned 32 bit integer in Q24.8 format (24 integer bits and 8 fractional bits).
// Output value of “24674867”represents 24674867/256 = 96386.2 Pa = 963.862 hPa
BMP280_U32_t BMP280_bmp280_compensate_P_int64(BMP280_S32_t adc_P, BMP280CalibrationData *cd) {
    BMP280_S64_t var1, var2, p;  // Temporary variables for intermediate results

    // Calculate var1 and var2 based on calibration coefficients and raw temperature (t_fine)
    var1 = ((BMP280_S64_t)t_fine) - 128000;
	var2 = var1 * var1 * (BMP280_S64_t)cd->dig_P6;
    var2 = var2 + ((var1 * (BMP280_S64_t)cd->dig_P5) << 17);
    var2 = var2 + (((BMP280_S64_t)cd->dig_P4) << 35);

    // Further calculations for var1
    var1 = ((var1 * var1 * (BMP280_S64_t)cd->dig_P3) >> 8) + ((var1 * (BMP280_S64_t)cd->dig_P2) << 12);
    var1 = (((((BMP280_S64_t)1) << 47) + var1) * (BMP280_S64_t)cd->dig_P1) >> 33;

    // Avoid division by zero if var1 is zero
    if (var1 == 0) {
        return 0;  // Return 0 to avoid division by zero exception
    }

    // Calculate the pressure based on the raw pressure (adc_P) and var1, var2
    p = 1048576 - adc_P;
    p = (((p << 31) - var2) * 3125) / var1;

    // Apply compensation for the pressure
    var1 = (((BMP280_S64_t)cd->dig_P9) * (p >> 13) * (p >> 13)) >> 25;
    var2 = (((BMP280_S64_t)cd->dig_P8) * p) >> 19;

    // Final pressure calculation and return in Q24.8 format (32-bit unsigned integer)
    p = ((p + var1 + var2) >> 8) + (((BMP280_S64_t)cd->dig_P7) << 4);
    
    // Return the compensated pressure value as an unsigned 32-bit integer
    return (BMP280_U32_t)p;
}

uint8_t BMP280_read(BMP280Data *data) {
	uint8_t buf[8] = {0};
	uint8_t reg;
	uint8_t err = BMP280_OK;
	BMP280CalibrationData cd;

	buf[0] = 0xf4; // control register
	buf[1] = 0x92; //presure and temperature measurement with 8x oversampling
	
	if(i2c_begin_transmisison(BMP280_ADDR, I2C_MODE_WRITE) != BMP280_OK) {
		i2c_end_transmisison();
		return BMP280_ERR_NOTFOUND;
	}
	err |= i2c_transmit_data(&buf, 2);
	i2c_end_transmisison();

	Delay_Ms(100); // Minimal delay is 25s when oversampling for data acuracity

	// Read pressure data	
	reg = 0xf7;
	err |= i2c_begin_transmisison(BMP280_ADDR, I2C_MODE_WRITE);
	err |= i2c_transmit_data(&reg, 1);
	i2c_end_transmisison();
	err |= i2c_begin_transmisison(BMP280_ADDR, I2C_MODE_READ);
	err |= i2c_receive_data(buf, 8);
	i2c_end_transmisison();

	if(err != I2C_OK)
		return err;

	// Merge to 20-bit variable
	BMP280_S32_t pressure;
	pressure |= buf[0] << 12;
	pressure |= buf[1] << 4;
	pressure |= (buf[2] >> 4) & 0x0f;
	
	BMP280_S32_t temperature;
	temperature |= buf[3] << 12;
	temperature |= buf[4] << 4;
	temperature |= (buf[5] >> 4) & 0x0f;

	// get callibrations
	reg = 0x88;
	err |= i2c_begin_transmisison(BMP280_ADDR, I2C_MODE_WRITE);
	err |= i2c_transmit_data(&reg, 1);
	i2c_end_transmisison();
	err |= i2c_begin_transmisison(BMP280_ADDR, I2C_MODE_READ);
	err |= i2c_receive_data(&cd, sizeof(cd));
	i2c_end_transmisison();

	if(err != I2C_OK)
		return err;

	data->temperature = BMP280_bmp280_compensate_T_int32(temperature, &cd);
	data->pressure = BMP280_bmp280_compensate_P_int64(pressure, &cd) >> 8;

	return BMP280_OK;
}	

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
		sprintf(buf, "%d", abs((int16_t)sensor_data.temperature));
		ssd1306_setbuf(0);
		if(sensor_data.temperature < 0) {
			ssd1306_fillRect(0, 15, 7, 3, 1);
		}
		ssd1306_drawstr_sz(8, 0, buf,1, fontsize_32x32);
		sprintf(buf, "%5d%%", (uint8_t)sensor_data.humidity);
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
	RCC->CFGR0 |= RCC_HPRE_DIV2;
	SystemInit();

	

	printf("Startup...\n");
    if(i2c_init() != I2C_OK)
		printf("Failed to init the I2C Bus\n");

	Delay_Ms(5);
	//ssd1306_init();

	drawClock();

	Delay_Ms(100);
	bmp280_errno = BMP280_read(&bmp280_data);
	aht20_errno = AHT20_read(&sensor_data);



	ssd1306_init();
	drawAll();

	
	printf("Stuck here forever...\n\r");
	while(1);
}