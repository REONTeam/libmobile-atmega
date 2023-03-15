#pragma once

#include <stdint.h>

#define SERIAL_5N1 0
#define SERIAL_6N1 _BV(UCSZ00)
#define SERIAL_7N1 _BV(UCSZ01)
#define SERIAL_8N1 _BV(UCSZ01) | _BV(UCSZ00)
#define SERIAL_5N2 _BV(USBS0)
#define SERIAL_6N2 _BV(USBS0) | _BV(UCSZ00)
#define SERIAL_7N2 _BV(USBS0) | _BV(UCSZ01)
#define SERIAL_8N2 _BV(USBS0) | _BV(UCSZ01) | _BV(UCSZ00)
#define SERIAL_5E1 _BV(UPM01)
#define SERIAL_6E1 _BV(UPM01) | _BV(UCSZ00)
#define SERIAL_7E1 _BV(UPM01) | _BV(UCSZ01)
#define SERIAL_8E1 _BV(UPM01) | _BV(UCSZ01) | _BV(UCSZ00)
#define SERIAL_5E2 _BV(UPM01) | _BV(USBS0)
#define SERIAL_6E2 _BV(UPM01) | _BV(USBS0) | _BV(UCSZ00)
#define SERIAL_7E2 _BV(UPM01) | _BV(USBS0) | _BV(UCSZ01)
#define SERIAL_8E2 _BV(UPM01) | _BV(USBS0) | _BV(UCSZ01) | _BV(UCSZ00)
#define SERIAL_5O1 _BV(UPM01) | _BV(UPM00)
#define SERIAL_6O1 _BV(UPM01) | _BV(UPM00) | _BV(UCSZ00)
#define SERIAL_7O1 _BV(UPM01) | _BV(UPM00) | _BV(UCSZ01)
#define SERIAL_8O1 _BV(UPM01) | _BV(UPM00) | _BV(UCSZ01) | _BV(UCSZ00)
#define SERIAL_5O2 _BV(UPM01) | _BV(UPM00) | _BV(USBS0)
#define SERIAL_6O2 _BV(UPM01) | _BV(UPM00) | _BV(USBS0) | _BV(UCSZ00)
#define SERIAL_7O2 _BV(UPM01) | _BV(UPM00) | _BV(USBS0) | _BV(UCSZ01)
#define SERIAL_8O2 _BV(UPM01) | _BV(UPM00) | _BV(USBS0) | _BV(UCSZ01) | _BV(UCSZ00)

void serial_putchar_inline(unsigned char c);
void serial_putchar(unsigned char c);
unsigned char serial_getchar_inline(void);
unsigned char serial_getchar(void);
unsigned serial_available(void);
void serial_drain(void);
void serial_init_config(unsigned long bauds, uint8_t config);
void serial_init(unsigned long bauds);
