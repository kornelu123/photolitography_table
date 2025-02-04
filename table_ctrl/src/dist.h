#pragma once

#include "pico/multicore.h"
#include "pico/stdlib.h" 
#include "pico/time.h" 

#include "hardware/pwm.h"

#define READ_TIMEOUT_MS 1000

#define TRIG_PIN 27
#define ECHO_PIN 28

#define US_TO_CM(x) (float)x/(float)58

extern volatile uint32_t dist;

uint32_t get_dist(void);
void dist_work(void);
void get_us_dist(void);
void init_dist(void);
