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
#include "lib_i2c.h"
#include "bomb.h"

#define AHT20_ADDR 0x38    // AHT20 I2C address (7-bit)
#define CMD_INIT 0x00     // Initialization command
#define CMD_MEASURE 0xAC  // Measure command


uint8_t battery_level = 40;

char buf[12];

void i2c_callback(uint8_t addr) {
    printf("Found i2c device %x \n", addr);
}

void drawAll() {
    for(int i = -20; i< 100; i++) {
        printf("Somebullshit\n");
        sprintf(buf, "%2d", abs(i));
        ssd1306_setbuf(0);
        if(i < 0) {
            ssd1306_fillRect(0, 15, 7, 3, 1);
        }
        ssd1306_drawstr_sz(8, 0, buf,1, fontsize_32x32);
        sprintf(buf, "%6s", "746mm");
        ssd1306_drawstr_sz(80, 12, buf,1, fontsize_8x8);
        sprintf(buf, "%6s", "22%");
        ssd1306_drawstr_sz(80, 24, buf,1, fontsize_8x8);

        ssd1306_fillRect(100, 0, 24, 8, 1);
        ssd1306_fillRect(124, 2, 2, 4, 1);
        ssd1306_fillRect(101, 1, 22, 6, 0);
        ssd1306_fillRect(102, 2, battery_level / 5, 4, 1);

        ssd1306_refresh();
        Delay_Ms(500);
        i2c_scan(i2c_callback);
        //read_aht20_temperature_humidity();
        
    }
    
}


#define I2C_MODE_READ 0
#define I2C_MODE_WRITE 1


#define AHT20_I2CADDR_DEFAULT 0x38   ///< AHT default i2c address
#define AHT20_CMD_CALIBRATE 0xBE     ///< Calibration command
#define AHT20_CMD_TRIGGER 0xAC       ///< Trigger reading command
#define AHT20_CMD_SOFTRESET 0xBA     ///< Soft reset command
#define AHT20_STATUS_BUSY 0x80       ///< Status bit for busy
#define AHT20_STATUS_CALIBRATED 0x08 ///< Status bit for calibrated

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
			printf("timeout waiting bus\n");
			return 1;
		}
	}

	I2C1->CTLR1 |= I2C_CTLR1_START;
	timeout = I2C_TIMEOUT;
	// Assert I2C master mode
	while((I2C1->STAR1 != 0x0001) || (I2C1->STAR2 != 0x0003)) // BUSY, MSL, SB flags
	if(--timeout < 0) {
		printf("I2C start timeout\n");
		return 2;
	}

	I2C1->DATAR = (addr << 1) & 0xFE;

	//Assert transmitter master mode
	timeout = I2C_TIMEOUT;
	while((I2C1->STAR1 != 0x0082) || (I2C1->STAR2 != 0x0007)) // BUSY, MSL, SB flags
	if(--timeout < 0) {
		printf("Transmit init timeout\n");
		return 2;
	}
	
	if(!transmit) {
		I2C1->CTLR1 |= I2C_CTLR1_ACK;

		// Start in transmitting mode
		I2C1->CTLR1 |= I2C_CTLR1_START;
		timeout = I2C_TIMEOUT;
		// Assert I2C master mode
		while((I2C1->STAR1 != 0x0001) || (I2C1->STAR2 != 0x0003)) // BUSY, MSL, SB flags
		if(--timeout < 0) {
			printf("I2C start in receiver mode timeout\n");
			return 2;
		}

		I2C1->DATAR = (addr << 1) | 0x01;
		//Assert receiver master mode
		timeout = I2C_TIMEOUT;
		while((I2C1->STAR1 != 0x0002) || (I2C1->STAR2 != 0x0003)) // BUSY, MSL, SB flags
		if(--timeout < 0) {
			printf("Receive init timeout\n");
			return 3;
		}

		//I2C1->CTLR1 |= I2C_CTLR1_ACK;	
	}
	return 0;
}

void i2c_end_transmisison() {
	I2C1->CTLR1 &= ~I2C_CTLR1_ACK;
	I2C1->CTLR1 |= I2C_CTLR1_STOP;
}

uint8_t i2c_transmit_byte(uint8_t data) {
	int32_t timeout = I2C_TIMEOUT;
	
	I2C1->DATAR = data;
	while(!(I2C1->STAR1 & I2C_STAR1_TXE));
	if(--timeout < 0) {
		printf("Send  timeout\n");
		return 3;
	}
	printf("Byte sent: 0x%02X\n", data);
	return 0;
}

uint8_t i2c_transmit_data(uint8_t *data, uint16_t length) {
	for (uint16_t i = 0; i < length; i++)
	{
		uint8_t status = i2c_transmit_byte(data[i]);
		if(status != 0) return status;
	}
}


uint8_t i2c_receive_byte(uint8_t *data) {
	int32_t timeout = I2C_TIMEOUT;

	while(!(I2C1->STAR1 & I2C_STAR1_RXNE))
	if(--timeout < 0) {
		printf("Receive byte timeout\n");
		return 3;
	}
	*data = I2C1->DATAR;
	printf("I2C data received: 0x%02x\n", *data);
	return 0;
}

uint8_t i2c_receive_data(uint8_t *data, uint16_t length) {
	for (uint16_t i = 0; i < length; i++)
	{
		uint8_t status = i2c_receive_byte((uint8_t*)(data+i));
		if(status != 0) return status;
	}
}

typedef struct AHT20SensorDataTypedef {
	uint16_t humidity_data;
	uint8_t humidity_temperature;
	uint16_t temperature_data;
	uint8_t crc_data;
} AHT20Data;

AHT20Data sensor_data;

uint8_t AHT20_read() {
	
	uint8_t buf[6] = {0};
	uint8_t err = 0;
	uint8_t status;
	Delay_Ms(200); // Delay on power on for AHT20
	
	// err |= i2c_begin_transmisison(0x38, I2C_MODE_WRITE);
	// err |= i2c_transmit_byte(0xbe);
	// i2c_end_transmisison();

	// // Soft reset
	// err |= i2c_begin_transmisison(AHT20_ADDR, I2C_MODE_WRITE);
	// err |= i2c_transmit_byte(AHT20_CMD_SOFTRESET);
	// i2c_end_transmisison();
	// err |= i2c_begin_transmisison(AHT20_ADDR, I2C_MODE_READ);
	// do {
	// 	//Delay_Ms(1);
	// 	i2c_receive_byte(&status);
		
	// 	printf("waiting for AHT soft reset, status byte is 0x%02X\n", status);
	// } while ((status & AHT20_STATUS_BUSY)); // until busy bit of status is 1
	// i2c_end_transmisison();
	// printf("--- Soft reset done\n");

	// Calibrate 
	buf[0] = AHT20_CMD_CALIBRATE;
	buf[1] = 0x08;
	buf[2] = 0x00;
	i2c_begin_transmisison(AHT20_ADDR, I2C_MODE_WRITE);
	i2c_transmit_data(buf, 3);
	i2c_end_transmisison();
	Delay_Ms(10);
	err |= i2c_begin_transmisison(AHT20_ADDR, I2C_MODE_READ);
	I2C1->CTLR1 &= ~I2C_CTLR1_ACK;
	i2c_receive_byte(&status);
	printf("Status byte is 0x%02X\n", status); 
	i2c_end_transmisison();
	printf("--- Calibration done\n");
	Delay_Ms(20);

	// Measure
	buf[0] = AHT20_CMD_TRIGGER;
	buf[1] = 0x33;
	buf[2] = 0x00; 
	err |= i2c_begin_transmisison(0x38, I2C_MODE_WRITE);
	err |= i2c_transmit_data(buf, 3);
	i2c_end_transmisison();
	Delay_Ms(100);
	printf("------ read result\n");
	err |= i2c_begin_transmisison(0x38, I2C_MODE_READ);
	
	
	while ((status & 0x80)) return 1; // until busy bit of status is 1 
	i2c_receive_data(buf, 6);
	i2c_end_transmisison();


    uint32_t temperature_raw = (((uint32_t)buf[3]) << 16) |
                           (((uint32_t)buf[4]) << 8) |
                           (((uint32_t)buf[5]) << 0);
	temperature_raw = temperature_raw & 0xFFFFF;
	float temperature = (float)(temperature_raw) 
                                 / 1048576.0f * 200.0f
                                 - 50.0f;


	//float temperature = (float)sensor_data.temperature_data / 65536.0 * 200.0 - 50.0;
	float humidity = ((float)sensor_data.humidity_data / 104857.0 )* 100.0;

	printf("state: 0x%02x TEMPERATURE: %d HUMIDITY: %d, sizeof %d\n", status, (uint16_t)temperature, humidity, sizeof(AHT20Data));

	return err;
}

int main()
{
	// 48MHz internal clock
	SystemInit();

    if(i2c_init(I2C_CLK_100KHZ) != I2C_OK) printf("Failed to init the I2C Bus\n");
    
	AHT20_read();
	//Delay_Ms(1500);
	//AHT20_read();

	// printf("-----------------");
	// Delay_Ms(3000);
	// printf("----Scanning I2C Bus for Devices---\n");
	// i2c_scan(i2c_callback);
	// printf("----Done Scanning----\n\n");
	// Delay_Ms( 100 );
    //     // Send the initialization command to the AHT20 (0xBE 0x00 0x00 0x00)
    // //uint8_t init_data[3] = {0xac, 0x33, 0x00};  // Initialization parameters
    // status = i2c_write(0x38, 0xbe, init_data, 0);
	// status = i2c_write(0x38, 0xbe, init_data, 3);
    // if (status != I2C_OK) {
    //     printf("Failed to initialize AHT20\n");

    // }

	// printf("sent something\n");

    // read_aht20_temperature_humidity();
	// printf("\r\r\n\ni2c_oled example\n\r");

	ssd1306_init();
	drawAll();
	// // init i2c and oled
	// //Delay_Ms( 100 );	// give OLED some more time
	// printf("initializing i2c oled...");
	
	printf("Stuck here forever...\n\r");
	while(1);
}