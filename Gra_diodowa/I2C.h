/*
 * I2C.h
 *
 * Created: 18.06.2020 15:44:56
 *  Author: Jeremiasz
 */ 


#ifndef I2C_H_
#define I2C_H_



void I2C_Initialize(uint16_t bitRate_kHz);

void I2C_start(void);

void I2C_stop(void);

void I2C_write(uint8_t dane);

uint8_t I2C_read(uint8_t ACK);


#endif /* I2C_H_ */