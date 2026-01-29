#define F_CPU 16000000UL
#define UART_BAUD_RATE 9600

#define is_time_ms(x) ((t0_cntr % (uint64_t)(x / 1.024f)) == 0) // Если предделитель равен 64
#define beep_on() beep(1)
#define beep_off() beep(0)

#ifndef __AVR_ATmega328P__
    #define __AVR_ATmega328P__
#endif


#include "libs/uart.h"
#include "libs/slave.h"

#include <avr/io.h>
#include <avr/interrupt.h>

#include <util/twi.h>

#include <stdbool.h>
#include <stdlib.h>
#include <string.h>


volatile uint64_t last_0 = 0;
volatile uint64_t last_1 = 0;
volatile uint64_t t0_cntr = 0;   // Счётчик переполнений
volatile uint64_t sys_time = 0;
volatile uint64_t timer_34 = 0;  // Счётчик перехода от 3 к 4 состоянию

volatile bool handset = 0;
volatile bool timer_main = 0; // Счётчик главного цикла
volatile bool timer_busy = 0; // Счётчик сигнала занято
volatile bool timer_ring = 0; // Счётчик сигнала посылки вызова

float frequency = 425;

bool last_st = 0;
bool once = 0;

uint8_t i = 0;
uint8_t num = 0;
volatile uint8_t *twi_buf;
char N_number[16];


volatile struct data_to_main {
    uint8_t st;
    char number[16];
} dtm;
volatile uint8_t* dtm_p = (uint8_t*) &dtm;