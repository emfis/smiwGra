#include <avr/io.h>
#include "avr/interrupt.h"
#include <util/delay.h>
#include <util/twi.h>
#include <avr/pgmspace.h>
#include <stdlib.h>
#include <stdbool.h>
#include "I2C.h"
#include "images.h"
#include "stdint.h"
#include "fonts.h"


#define LED1 (1<<PB1)
#define LED2 (1<<PB0)
#define LED3 (1<<7)
#define LED4 (1<<4)
#define LED5 (1<<3)

#define BUTTON1 (1<<1)
#define BUTTON2 (1<<0)
#define BUTTON3 (1<<6)
#define BUTTON4 (1<<5)
#define BUTTON5 (1<<2)


#define INT_PIN (1<<PD2)
#define BUZZER_PIN (1<<PD5)

#define PCFaddrW 0x74 //zapis expandera
#define PCFaddrR 0x75 //odczyt expandera
#define DISPaddrW 0x78
#define DISPaddrW2 0x7A
#define DISPaddrW3 0x79
#define PCFwriteBit 0x01
#define PCFreadBit 0x00
#define ACK 1
#define NACK 0

uint8_t ledBuffer;
uint8_t I2COutBuffer;
bool PCFintFlag = 0;
bool gameFlag = 0;
uint8_t intCount = 0;
int points = 0;

uint8_t timer2Time = 0;
uint8_t clickTimeFlag = 0;
// 0 - brak klikni�cia, warto�� po inicjalizacji
// 1 - klikni�to w czasie
// 2 - klikni�to po czasie, timer przepe�niony

uint8_t button1IsClicked = 0;
uint8_t button2IsClicked = 0;
uint8_t button3IsClicked = 0;
uint8_t button4IsClicked = 0;
uint8_t button5IsClicked = 0;
uint8_t buttonMask = 0b01100111;
//jedynki - adresy wyj�ciowe uk�adu PCF z podpi�tymi przyciskami
//zera - diody

bool led1ON = 0;
bool led2ON = 0;
bool led3ON = 0;
bool led4ON = 0;
bool led5ON = 0;

const uint8_t *ssd1306tx_font_src;
uint8_t ssd1306tx_font_char_base;


void displayNumber(uint8_t number)
{
	char displayString[3];
	itoa(number, displayString, 10);
	//ssd1306_clear_display();
	ssd1306tx_stringxy(ssd1306xled_font8x16data, 0, 0, displayString);
}

uint8_t readButtons()
{
		PCFintFlag = 0;
		uint8_t buffer; //inicjalizuje buffer do zapisu stanu
		I2C_start(); //zaczynam komunikacje na i2c
		I2C_write(PCFaddrR);
		buffer = I2C_read(ACK); //zapisuje do buffera
		I2C_stop();//koncze komunikacje
		return buffer;//zwracam odczytane wartosci
		
}

const uint8_t ssd1306_init_sequence [] PROGMEM = {	
  
  // Inicjalizacja wyswietlacza
	0xAE,			// Ustaw wystwietlacz OFF
	0xD5, 0xF0,		// Set display clock divide ratio/oscillator frequency, set divide ratio
	0xA8, 0x3F,		// Set multiplex ratio (1 to 64) ... (height - 1)
	0xD3, 0x00,		// Set display offset. 00 = no offset
	0x40 | 0x00,	// Set start line address, at 0.
	0x8D, 0x14,		// Charge Pump Setting, 14h = Enable Charge Pump
	0x20, 0x00,		// Set Memory Addressing Mode - 00=Horizontal, 01=Vertical, 10=Page, 11=Invalid
	0xA0 | 0x01,	// Set Segment Re-map
	0xC8,			// Set COM Output Scan Direction
	0xDA, 0x12,		// Set COM Pins Hardware Configuration - 128x32:0x02, 128x64:0x12
	0x81, 0x3F,		// Set contrast control register
	0xD9, 0x22,		// Set pre-charge period (0x22 or 0xF1)
	0xDB, 0x20,		// Set Vcomh Deselect Level - 0x00: 0.65 x VCC, 0x20: 0.77 x VCC (RESET), 0x30: 0.83 x VCC
	0xA4,			// Entire Display ON (resume) - output RAM to display
	0xA6,			// Set Normal/Inverse Display mode. A6=Normal; A7=Inverse
	0x2E,			// Deactivate Scroll command
	0xAF,			// Ustaw wyswietlacz OFF
	//
	0x22, 0x00, 0x3f,	// Set Page Address (start,end)
	0x21, 0x00,	0x7f,	// Set Column Address (start,end)
	
};

void ssd1306_start_command(void) {
	I2C_start();
	I2C_write(DISPaddrW);	// Slave address: R/W(SA0)=0 - write
	I2C_write(0x00);			// Control byte: D/C=0 - write command
}

void ssd1306_start_data(void) {
	I2C_start();
	I2C_write(DISPaddrW);	// Slave address, R/W(SA0)=0 - write
	I2C_write(0x40);			// Control byte: D/C=1 - write data
}

void ssd1306_init(void) {
	ssd1306_start_command();	// Initiate transmission of command
	for (uint8_t i = 0; i < sizeof (ssd1306_init_sequence); i++) {
		I2C_write(pgm_read_byte(&ssd1306_init_sequence[i]));	// Send the command out
	}
	I2C_stop();	// Finish transmission
}

void ssd1306_setpos(uint8_t x, uint8_t y) {
	ssd1306_start_command();
	I2C_write(0xb0 | (y & 0x07));	// Set page start address
	I2C_write(x & 0x0f);			// Set the lower nibble of the column start address
	I2C_write(0x10 | (x >> 4));		// Set the higher nibble of the column start address
	I2C_stop();	// Finish transmission
}

void ssd1306_fill4(uint8_t p1, uint8_t p2, uint8_t p3, uint8_t p4) {
	ssd1306_setpos(0, 0);
	ssd1306_start_data();	// Initiate transmission of data
	for (uint16_t i = 0; i < 128 * 8 / 4; i++) {
		I2C_write(p1);
		I2C_write(p2);
		I2C_write(p3);
		I2C_write(p4);
	}
	I2C_stop();	// Finish transmission
}

void ssd1306_data_byte(uint8_t b) {
	I2C_write(b);
}

void ssd1306_stop(void) {
	I2C_stop();
}

void ssd1306_draw_bmp(uint8_t x0, uint8_t y0, uint8_t x1, uint8_t y1, const uint8_t bitmap[]) {
	uint16_t j = 0;
	uint8_t y;
	if (y1 % 8 == 0) y = y1 / 8;
	else y = y1 / 8 + 1;
	for (y = y0; y < y1; y++) {
		ssd1306_setpos(x0, y);
		ssd1306_start_data();
		for (uint8_t x = x0; x < x1; x++) {
			ssd1306_data_byte(pgm_read_byte(&bitmap[j++]));
		}
		
	}
}

void ssd1306_clear_display(void)
{
	uint8_t x0 = 0;
	uint8_t x1 = 128;
	uint8_t y0 = 0;
	uint8_t y1 = 8;
	int16_t j = 0;
	uint8_t y;
	if (y1 % 8 == 0) y = y1 / 8;
	else y = y1 / 8 + 1;
	for (y = y0; y < y1; y++) {
		ssd1306_setpos(x0, y);
		ssd1306_start_data();
		for (uint8_t x = x0; x < x1; x++) {
			ssd1306_data_byte(0x00);
		}
	
}
}

void ssd1306tx_init(const uint8_t *fron_src, uint8_t char_base) {
	ssd1306tx_font_src = fron_src;
	ssd1306tx_font_char_base = char_base;
}

// ----------------------------------------------------------------------------

void ssd1306tx_char(char ch) {
	uint16_t j = (ch << 2) + (ch << 1) - 192; // Equiv.: j=(ch-32)*6 <== Convert ASCII code to font data index.
	ssd1306_start_data();
	for (uint8_t i = 0; i < 6; i++) {
		ssd1306_data_byte(pgm_read_byte(&ssd1306tx_font_src[j + i]));
	}
	ssd1306_stop();
}

void ssd1306tx_string(char *s) {
	while (*s) {
		ssd1306tx_char(*s++);
	}
}

void ssd1306tx_stringxy(const uint8_t *fron_src, uint8_t x, uint8_t y, const char s[]) {
	uint16_t j, k = 0;
	while (s[k] != '\0') {
		j = s[k] * 16 - (32 * 16); // Convert ASCII code to font data index. NOTE: (x*16) already optimized to (x<<4).
		if (x > 120) {
			x = 0;    // Go to the next line.
			y++;
		}
		ssd1306_setpos(x, y);
		ssd1306_start_data();
		for (uint8_t i = 0; i < 8; i++) {
			ssd1306_data_byte(pgm_read_byte(&fron_src[j + i]));
		}
		ssd1306_stop();
		ssd1306_setpos(x, y + 1);
		ssd1306_start_data();
		for (uint8_t i = 0; i < 8; i++) {
			ssd1306_data_byte(pgm_read_byte(&fron_src[j + i + 8]));
		}
		ssd1306_stop();
		x += 8;
		k++;
	}
}

void PCFwrite(uint8_t buffer)
{
	I2C_start();	
	I2C_write(PCFaddrW);
	I2C_write(buffer);
	I2C_stop();
}

void game()
{
	points = 0; // ilosc poczatkowych punktow
	_delay_ms(700); // czekamy 700 ms
	char displayString[10]; //rezerwuje pamiec na string 10 znakow
	bool breakFlag = 1; //czy wcisnieto przycisk
	uint8_t buffer = 0; //inicjalizacja buffera na odczyt stanow przyciskow
	ssd1306_clear_display(); //czyszcze wyswietlacz
	ssd1306tx_stringxy(ssd1306xled_font8x16data, 50 , 0, "Game"); //wyswietlam "Game"
	ssd1306tx_stringxy(ssd1306xled_font8x16data, 30 , 4, "Points: ");//pod spodem wyswietlam "Points"
	itoa(points, displayString, 10); // konwertuje int points na ascii i wpisuje do zmiennej displayString
	ssd1306tx_stringxy(ssd1306xled_font8x16data, 60 , 6, displayString); // wyswietlam aktualna ilosc punktow
	PCFwrite(0b11111111); //uruchamiam metode PCFWrite nadajac bity
	PORTB |= LED1 | LED2; //wyłączenie diod podpiętycj pod port
	PCFintFlag = 0;// //wyzerowanie flagi przerwania od expandera
	PORTB &= ~(LED2);// włączenie led2
	while(breakFlag)
	{
		if(PCFintFlag)
		{
			buffer = readButtons(); //odczytujemy stan przyciskow
			if((~buffer & BUTTON2)) // sprawdzamy czy zostal wcisniety drugi przycisk
			{
				points++; //dodajemy punkt
				itoa(points, displayString, 10);//konwertujemy do ascii
				ssd1306tx_stringxy(ssd1306xled_font8x16data, 60, 6, "  "); //czyscimy miejsce w ktorym bedziemy wyswietlac
				ssd1306tx_stringxy(ssd1306xled_font8x16data, 60, 6, displayString); //wyswietlamy nowa wartosc punkotw
				PORTB |= (LED2); // wyłączenie led2
				breakFlag = 0; // wychodzimy z petli
			}
			else
			{
				points--; //odejmujemy punkt
				itoa(points, displayString, 10);//konwertujemy do ascii
				ssd1306tx_stringxy(ssd1306xled_font8x16data, 60, 6, "  ");//czyscimy miejsce w ktorym bedziemy wyswietlac
				ssd1306tx_stringxy(ssd1306xled_font8x16data, 60, 6, displayString); //wyswietlamy nowa wartosc punkotw
				PORTB |= (LED2); //wyłączenie led2
				breakFlag = 0;// wychodzimy z petli
			}
		}
		_delay_ms(1); // jesli nie nastapilo przerwanie odczekujemy 1 ms i sprawdzamy ponownie stan PCFintFlag
		//PORTB ^= LED2;
	}
	PORTD ^= BUZZER_PIN; //zmiana stanu logicznego na pinie z podpiętym buzzerem
	_delay_ms(500); //emitujemy dzwiek przez pol sekundy
	PORTD ^= BUZZER_PIN; // wylaczamy buzzer

  //resetujemy stany flag do ponownej petli
	breakFlag = 1;
	PCFintFlag = 0;
	buffer = 0;
	PCFwrite(0b01111111); // wlaczamy diode 
	while(breakFlag) //powtarzamy petle tym razem oczekujac na inny przycisk
	{
		if(PCFintFlag)
		{
			buffer = readButtons();
			if((~buffer & BUTTON3))
			{
				PCFwrite(0b11111111);
				points++;
				itoa(points, displayString, 10);
				ssd1306tx_stringxy(ssd1306xled_font8x16data, 60, 6, "  ");
				ssd1306tx_stringxy(ssd1306xled_font8x16data, 60, 6, displayString);
				breakFlag = 0;
			}
			else
			{
				PCFwrite(0b11111111);
				points--;
				itoa(points, displayString, 10);
				ssd1306tx_stringxy(ssd1306xled_font8x16data, 60, 6, "  ");
				ssd1306tx_stringxy(ssd1306xled_font8x16data, 60, 6, displayString);
				breakFlag = 0;
			}
		}
		//todo variable and delay for diod switching
		_delay_ms(1);
	}
	PORTD ^= BUZZER_PIN;
	_delay_ms(500);
	PORTD ^= BUZZER_PIN;
	breakFlag = 1;
	PCFintFlag = 0;
	buffer = 0;
	PCFwrite(0b01111111);
	while(breakFlag)
	{
		if(PCFintFlag)
		{
			buffer = readButtons();
			if((~buffer & BUTTON3))
			{
				PCFwrite(0b11111111);
				points++;
				itoa(points, displayString, 10);
				ssd1306tx_stringxy(ssd1306xled_font8x16data, 60, 6, "  ");
				ssd1306tx_stringxy(ssd1306xled_font8x16data, 60, 6, displayString);
				breakFlag = 0;
			}
			else
			{
				PCFwrite(0b11111111);
				points--;
				itoa(points, displayString, 10);
				ssd1306tx_stringxy(ssd1306xled_font8x16data, 60, 6, "  ");
				ssd1306tx_stringxy(ssd1306xled_font8x16data, 60, 6, displayString);
				breakFlag = 0;
			}
		}
		_delay_ms(1);
	}
	
		PORTD ^= BUZZER_PIN;
		_delay_ms(500);
		PORTD ^= BUZZER_PIN;
		breakFlag = 1;
		PCFintFlag = 0;
		buffer = 0;
		PCFwrite(0b11110111);
		while(breakFlag)
		{
			if(PCFintFlag)
			{
				buffer = readButtons();
				if((~buffer & BUTTON5))
				{
					PCFwrite(0b11111111);
					points++;
					itoa(points, displayString, 10);
					ssd1306tx_stringxy(ssd1306xled_font8x16data, 60, 6, "  ");
					ssd1306tx_stringxy(ssd1306xled_font8x16data, 60, 6, displayString);
					breakFlag = 0;
				}
				else
				{
					PCFwrite(0b11111111);
					points--;
					itoa(points, displayString, 10);
					ssd1306tx_stringxy(ssd1306xled_font8x16data, 60, 6, "  ");
					ssd1306tx_stringxy(ssd1306xled_font8x16data, 60, 6, displayString);
					breakFlag = 0;
				}
			}
			_delay_ms(1);
		}
				PORTD ^= BUZZER_PIN;
				_delay_ms(500);
				PORTD ^= BUZZER_PIN;
				breakFlag = 1;
				PCFintFlag = 0;
				buffer = 0;
				PCFwrite(0b11101111);
				while(breakFlag)
				{
					if(PCFintFlag)
					{
						buffer = readButtons();
						if((~buffer & BUTTON4))
						{
							PCFwrite(0b11111111);
							points++;
							itoa(points, displayString, 10);
							ssd1306tx_stringxy(ssd1306xled_font8x16data, 60, 6, "  ");
							ssd1306tx_stringxy(ssd1306xled_font8x16data, 60, 6, displayString);
							breakFlag = 0;
						}
						else
						{
							PCFwrite(0b11111111);
							points--;
							itoa(points, displayString, 10);
							ssd1306tx_stringxy(ssd1306xled_font8x16data, 60, 6, "  ");
							ssd1306tx_stringxy(ssd1306xled_font8x16data, 60, 6, displayString);
							breakFlag = 0;
						}
					}
					_delay_ms(1);
				}
	PORTD ^= BUZZER_PIN;			
	_delay_ms(500);
	PORTD ^= BUZZER_PIN;
	breakFlag = 1;
	PCFintFlag = 0;
	buffer = 0;
	PCFwrite(0b01111111);
	while(breakFlag)
	{
		if(PCFintFlag)
		{
			buffer = readButtons();
			if((~buffer & BUTTON3))
			{
				PCFwrite(0b11111111);
				points++;
				itoa(points, displayString, 10);
				ssd1306tx_stringxy(ssd1306xled_font8x16data, 60, 6, "  ");
				ssd1306tx_stringxy(ssd1306xled_font8x16data, 60, 6, displayString);
				breakFlag = 0;
			}
			else
			{
				PCFwrite(0b11111111);
				points--;
				itoa(points, displayString, 10);
				ssd1306tx_stringxy(ssd1306xled_font8x16data, 60, 6, "  ");
				ssd1306tx_stringxy(ssd1306xled_font8x16data, 60, 6, displayString);
				breakFlag = 0;
			}
		}
		_delay_ms(1);
	}
		PORTD ^= BUZZER_PIN;
		_delay_ms(500);
		PORTD ^= BUZZER_PIN;
		breakFlag = 1;
		PCFintFlag = 0;
		buffer = 0;
		PORTB &= ~(LED1);
		PCFwrite(0b11111111);
		while(breakFlag)
		{
			if(PCFintFlag)
			{
				buffer = readButtons();
				if((~buffer & BUTTON1))
				{
					points++;
					itoa(points, displayString, 10);
					ssd1306tx_stringxy(ssd1306xled_font8x16data, 60, 6, "  ");
					ssd1306tx_stringxy(ssd1306xled_font8x16data, 60, 6, displayString);
					PORTB |= (LED1);
					breakFlag = 0;
				}
				else
				{
					points--;
					itoa(points, displayString, 10);
					ssd1306tx_stringxy(ssd1306xled_font8x16data, 60, 6, "  ");
					ssd1306tx_stringxy(ssd1306xled_font8x16data, 60, 6, displayString);
					PORTB |= (LED1);
					breakFlag = 0;
				}
			}
			_delay_ms(1);
			//PORTB ^= LED2;
		}
				PORTD ^= BUZZER_PIN;
				_delay_ms(500);
				PORTD ^= BUZZER_PIN;
				breakFlag = 1;
				PCFintFlag = 0;
				buffer = 0;
				PORTB &= ~(LED2);
				PCFwrite(0b11111111);
				while(breakFlag)
				{
					if(PCFintFlag)
					{
						buffer = readButtons();
						if((~buffer & BUTTON2))
						{
							points++;
							itoa(points, displayString, 10);
							ssd1306tx_stringxy(ssd1306xled_font8x16data, 60, 6, "  ");
							ssd1306tx_stringxy(ssd1306xled_font8x16data, 60, 6, displayString);
							PORTB |= (LED2);
							breakFlag = 0;
						}
						else
						{
							points--;
							itoa(points, displayString, 10);
							ssd1306tx_stringxy(ssd1306xled_font8x16data, 60, 6, "  ");
							ssd1306tx_stringxy(ssd1306xled_font8x16data, 60, 6, displayString);
							PORTB |= (LED2);
							breakFlag = 0;
						}
					}
					_delay_ms(1);
				}
						PORTD ^= BUZZER_PIN;
						_delay_ms(500);
						PORTD ^= BUZZER_PIN;
						breakFlag = 1;
						PCFintFlag = 0;
						buffer = 0;
						PCFwrite(0b11110111);
						while(breakFlag)
						{
							if(PCFintFlag)
							{
								buffer = readButtons();
								if((~buffer & BUTTON5))
								{
									PCFwrite(0b11111111);
									points++;
									itoa(points, displayString, 10);
									ssd1306tx_stringxy(ssd1306xled_font8x16data, 60, 6, "  ");
									ssd1306tx_stringxy(ssd1306xled_font8x16data, 60, 6, displayString);
									breakFlag = 0;
								}
								else
								{
									PCFwrite(0b11111111);
									points--;
									itoa(points, displayString, 10);
									ssd1306tx_stringxy(ssd1306xled_font8x16data, 60, 6, "  ");
									ssd1306tx_stringxy(ssd1306xled_font8x16data, 60, 6, displayString);
									breakFlag = 0;
								}
							}
							_delay_ms(1);
						}
										PORTD ^= BUZZER_PIN;
										_delay_ms(500);
										PORTD ^= BUZZER_PIN;
										breakFlag = 1;
										PCFintFlag = 0;
										buffer = 0;
										PCFwrite(0b11101111);
										while(breakFlag)
										{
											if(PCFintFlag)
											{
												buffer = readButtons();
												if((~buffer & BUTTON4))
												{
													PCFwrite(0b11111111);
													points++;
													itoa(points, displayString, 10);
													ssd1306tx_stringxy(ssd1306xled_font8x16data, 60, 6, "  ");
													ssd1306tx_stringxy(ssd1306xled_font8x16data, 60, 6, displayString);
													breakFlag = 0;
												}
												else
												{
													PCFwrite(0b11111111);
													points--;
													itoa(points, displayString, 10);
													ssd1306tx_stringxy(ssd1306xled_font8x16data, 60, 6, "  ");
													ssd1306tx_stringxy(ssd1306xled_font8x16data, 60, 6, displayString);
													breakFlag = 0;
												}
											}
											_delay_ms(1);
										}
										
										ssd1306tx_stringxy(ssd1306xled_font8x16data, 40, 0, "Game Over");
										//ssd1306tx_stringxy(ssd1306xled_font8x16data, 0, 2, "Click to play");
										PCFintFlag = 0;
										breakFlag = 1;
										_delay_ms(500);
										PORTD ^= BUZZER_PIN;
										_delay_ms(500);
										PORTD ^= BUZZER_PIN;
										_delay_ms(500);
										PORTD ^= BUZZER_PIN;
										_delay_ms(500);
										PORTD ^= BUZZER_PIN;
										

										
}

int main(void)
{

	while (1) 
    { 
      //zapalam wszystkie diody zeby zobaczyc czy napewno dzialaja
			PCFwrite(0b11111111);
			_delay_ms(100);
			PORTB &= ~LED1;
			_delay_ms(100);
			PORTB &= ~LED2;
			_delay_ms(100);
			PCFwrite(~(LED3) | buttonMask);
			_delay_ms(100);
			PCFwrite(~(LED3 | LED4) | buttonMask);
			_delay_ms(100);
			PCFwrite(~(LED3 | LED4 | LED5) | buttonMask);
			_delay_ms(100);
			PCFwrite(0b11111111);
			PORTB |= LED1 | LED2;
			_delay_ms(100);
			game();
			if (PCFintFlag == 1) //nastapilo przerwanie! odczytuje stan przyciskow
			{
				readButtons();
			} 
			_delay_ms(1);

    }
}

ISR (INT0_vect)
{
	PCFintFlag = 1;
}