/*
 * I2C.c
 *
 * Created: 18.06.2020 15:44:40
 *  Author: Jeremiasz
 */ 

#include "stdint.h"
#include <avr/io.h>
#define LED5 (1<<PB0)
void I2C_Initialize(uint16_t bitRate_kHz)
{
	//TWSR |= 0x00; // Set prescaler to 1

		uint8_t bitrate_div;

		bitrate_div = ((F_CPU/1000l)/bitRate_kHz);
		
		if(bitrate_div >= 16)
		bitrate_div = (bitrate_div-16)/2;
		
		TWBR = bitrate_div;
}

void I2C_start(void){
	TWCR = (1<<TWINT) | (1<<TWEN) |( 1<<TWSTA);
	while (! (TWCR & (1<<TWINT)));
}

void I2C_stop(void){
	TWCR = (1<<TWINT) | (1<<TWEN) | (1<<TWSTO);
	while (! (TWCR & (1<<TWSTO)));
}

void I2C_write(uint8_t dane){
	TWDR = dane;
	TWCR = (1<<TWINT) | (1<<TWEN);
	while (! (TWCR & (1<<TWINT)));
}

uint8_t I2C_read(uint8_t ACK){
	TWCR = (1<<TWINT) | (ACK<<TWEA) | (1<<TWEN);
	while (! (TWCR & (1<<TWINT)));
	return TWDR;
}