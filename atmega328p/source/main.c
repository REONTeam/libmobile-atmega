#include <stdint.h>
#include <stdio.h>
#include <avr/interrupt.h>
#include <avr/eeprom.h>
#include <avr/pgmspace.h>
#include <util/atomic.h>
#include <util/delay.h>

#include <mobile.h>
#include <mobile_data.h>

#include "utils.h"
#include "pins.h"
#include "timer.h"
#include "serial.h"

#include "gbridge.h"
#include "gbridge_prot_ma.h"

// A stack canary is a value that will be checked periodically
// This allows making sure the stack doesn't overflow into used data
#define STACK_SIZE 0x180
#define STACK_CANARY *(uint32_t *)(RAMEND - STACK_SIZE - 1)
#define STACK_CANARY_VAL 0xAAAAAAAA

// Define this to print every byte sent and received
//#define DEBUG_SPI

// Define this to print every command sent and received
//#define DEBUG_CMD

struct mobile_adapter adapter;

uint32_t micros_latch[MOBILE_MAX_TIMERS] = {0};

#ifdef DEBUG_SPI
#define BUF_LEN 0x100
volatile unsigned char buffer[BUF_LEN];
volatile unsigned buf_in = 0;
volatile unsigned buf_out = 0;

int buffer_isempty(void)
{
    return buf_in == buf_out;
}

int buffer_isfull(void)
{
    return (buf_in + 1) % BUF_LEN == buf_out;
}

void buffer_put(unsigned char c)
{
    volatile unsigned in = buf_in;
    buffer[in++] = c;
    buf_in = in % BUF_LEN;
}

unsigned char buffer_get(void)
{
    volatile unsigned out = buf_out;
    unsigned char c = buffer[out++];
    buf_out = out % BUF_LEN;
    return c;
}

char last_SPDR = MOBILE_SERIAL_IDLE_BYTE;
#endif

void mobile_impl_debug_log(void *user, const char *line)
{
    (void)user;
#ifdef DEBUG_CMD
    printf_P(PSTR("%s\r\n"), line);
#elif !defined(DEBUG_SPI)
    gbridge_cmd_debug_line(line);
#endif
}

void mobile_impl_serial_disable(void *user)
{
    (void)user;
    SPCR = SPSR = 0;
}

void mobile_impl_serial_enable(void *user, bool mode_32bit)
{
    (void)user;
    (void)mode_32bit;
    pinmode(PIN_SPI_MISO, OUTPUT);
    SPCR = _BV(SPE) | _BV(SPIE) | _BV(CPOL) | _BV(CPHA);
    SPSR = 0;
    SPDR = MOBILE_SERIAL_IDLE_BYTE;
}

bool mobile_impl_config_read(void *user, void *dest, uintptr_t offset, size_t size)
{
    (void)user;
    eeprom_read_block(dest, (void *)offset, size);
    return true;
}

bool mobile_impl_config_write(void *user, const void *src, uintptr_t offset, size_t size)
{
    (void)user;
    eeprom_write_block(src, (void *)offset, size);
    return true;
}

void mobile_impl_time_latch(void *user, unsigned timer)
{
    (void)user;
    micros_latch[timer] = timer_get();
}

bool mobile_impl_time_check_ms(void *user, unsigned timer, unsigned ms)
{
    (void)user;
    return (timer_get() - micros_latch[timer]) > ((uint32_t)ms * 1000);
}

int main(void)
{
    // Install stack canary
    STACK_CANARY = STACK_CANARY_VAL;

    // Initialize
    timer_init();
    serial_init(500000);
    mobile_init(&adapter, NULL);

    // Reset configs
    /*
    eeprom_write_block(&(char []){0}, (void *)0x000, 1);
    eeprom_write_block(&(char []){0}, (void *)0x100, 1);
    */

    // Write config
    /*
    mobile_config_load(&adapter);
    mobile_config_set_dns(&adapter, (struct mobile_addr *)&(struct mobile_addr4){
        .type = MOBILE_ADDRTYPE_IPV4,
        .port = 5353,
        .host = {127, 0, 0, 1}
    }, NULL);
    mobile_config_set_relay(&adapter, (struct mobile_addr *)&(struct mobile_addr4){
        .type = MOBILE_ADDRTYPE_IPV4,
        .port = MOBILE_DEFAULT_RELAY_PORT,
        .host = {x, x, x, x}
    });
    mobile_config_save(&adapter);
    pinmode(PIN_LED, OUTPUT);
    for(;;) {
        writepin(PIN_LED, HIGH); _delay_ms(100);
        writepin(PIN_LED, LOW); _delay_ms(100);
    }
    */

#if !defined(DEBUG_SPI) && !defined(DEBUG_CMD)
    gbridge_init();
    gbridge_prot_ma_init();
#endif

    // Set up timer 0
    TCNT0 = 0;
    TCCR0A = 0;
    TCCR0B = _BV(CS01) | _BV(CS00);  // Prescale by 1/64
    TIMSK0 = _BV(TOIE0);  // Enable the interrupt

    sei();

#if defined(DEBUG_SPI) || defined(DEBUG_CMD)
    printf_P(PSTR("----\r\n"));
#endif

    mobile_start(&adapter);
    for (;;) {
        mobile_loop(&adapter);

#if !defined(DEBUG_SPI) && !defined(DEBUG_CMD)
        gbridge_loop();
        gbridge_prot_ma_loop();
#endif

#ifdef DEBUG_SPI
        if (!buffer_isempty()) {
            printf_P(PSTR("%02X\t"), buffer_get());
            while (buffer_isempty());
            printf_P(PSTR("%02X\r\n"), buffer_get());
        }
#endif
    }
}

ISR (SPI_STC_vect)
{
#ifdef DEBUG_SPI
    if (!buffer_isfull()) buffer_put(SPDR);
    if (!buffer_isfull()) buffer_put(last_SPDR);
    SPDR = last_SPDR = mobile_transfer(&adapter, SPDR);
#else
    SPDR = mobile_transfer(&adapter, SPDR);
#endif
}

ISR (TIMER0_OVF_vect)
{
    // Hang if the stack canary has been tripped
    if (STACK_CANARY != STACK_CANARY_VAL) {
        // Disable SPI and blink the led
        SPCR = 0;
        pinmode(PIN_LED, OUTPUT);
        for(;;) {
            writepin(PIN_LED, HIGH);
            _delay_ms(100);
            writepin(PIN_LED, LOW);
            _delay_ms(100);
        }
    }

    timer_isr();
}
