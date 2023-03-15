#include "timer.h"

#include <util/atomic.h>

volatile uint32_t micros;

void timer_init(void)
{
    // Stop timer 0
    TCCR0B = 0;

    micros = 0;

    // Set up timer 0
    TCNT0 = 0;
    TCCR0A = 0;
    TCCR0B = _BV(CS01) | _BV(CS00);  // Prescale by 1/64
    TIMSK0 = _BV(TOIE0);  // Enable the interrupt
}

uint32_t timer_get(void)
{
    uint32_t time;
    ATOMIC_BLOCK(ATOMIC_FORCEON) {
        time = micros;
    }
    return time;
}

void timer_isr(void)
{
    // Overflows every 64 (prescaler) * 256 (overflow) cycles
    // F_CPU is in cycles/second
    // Given the used values, overflows every 1.024 ms.
    micros += (64 * 256) / (F_CPU / 1000000L);
    // TODO: Use Timer CTC mode to interrupt every 1ms exactly
}
