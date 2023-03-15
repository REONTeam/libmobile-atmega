#pragma once

#include <stdint.h>

void timer_init(void);
uint32_t timer_get(void);
void timer_isr(void);
