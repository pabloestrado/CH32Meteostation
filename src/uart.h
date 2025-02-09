
#include "ch32v003fun.h"

#define UART_BAUDRATE 9600

volatile char command[32] = {0};
volatile uint8_t command_index = 0;
volatile uint8_t command_received = 0;


void USART1_IRQHandler(void) __attribute__((interrupt));
void USART1_IRQHandler(void)
{

	if(USART1->STATR & USART_STATR_RXNE)
	{
		//Check for errors (overrun, framing, noise)
		//Check is command_recived is not processed by code and ignore future commands
        if (USART1->STATR & (USART_STATR_ORE | USART_STATR_FE | USART_STATR_NE)) {
            // Clear all errors and discart recevided byte, do not trust corrupted data
            USART1->STATR &= ~(USART_STATR_ORE | USART_STATR_FE | USART_STATR_NE | USART_STATR_RXNE) ;
        }
		else {
			// Read from the DATAR Register to reset the flag
			char received = (char)USART1->DATAR;
			if(received == '\n' || received == '\r') {
				command[command_index] = 0; //replace newline with end of string
				command_index = 0;
				command_received = 1;
			}
			else {
				command[command_index] = received;
				if(command_index >= (sizeof(command) - 1)) {
					command_index = 0;
				}
				else {
					command_index++;
				}
			}
		}
	}
}

// If command is received with uart
uint8_t is_command_received() {
	return command_received;
}

// Acknovledge command is processed, ready for new command
void command_ack() {
	command_received = 0;
}

//return command array
char* get_command() {
	return command;
}

void uart_init() {
    RCC->APB2PCENR |= RCC_APB2Periph_USART1;
	// Enable the UART GPIO Port, and the Alternate Function IO Flag

    funGpioInitAll();
    funPinMode(PD5, GPIO_Speed_10MHz | GPIO_CNF_OUT_PP_AF); // TX pin push-pull
    funPinMode(PD6, GPIO_CNF_IN_FLOATING); //  RX pin floating

	// Set CTLR1 Register (Enable RX & TX, set Word Length and Parity)
	USART1->CTLR1 = USART_Mode_Tx | USART_Mode_Rx | USART_CTLR1_RXNEIE;
	// Set CTLR2 Register (Stopbits)
	USART1->CTLR2 = 0x00;
	// Set CTLR3 Register (Flow control)
	USART1->CTLR3 = 0x00;
	
	// Set the Baudrate, assuming 48KHz
	USART1->BRR = (FUNCONF_SYSTEM_CORE_CLOCK / (UART_BAUDRATE * 16)) << 4;

	// Enable the UART RXNE Interrupt
    NVIC_EnableIRQ(USART1_IRQn);
	
	// Enable the UART
	USART1->CTLR1 |= USART_CTLR1_UE;
}

void uart_send(uint8_t *data, uint16_t length) {
    for(uint16_t cnt = 0; cnt < length; cnt++) {
        while(!(USART1->STATR & USART_FLAG_TC));
            USART1->DATAR = data[cnt];
    }
}

void uart_print(char *str) {
	uart_send(str, strlen(str));
}