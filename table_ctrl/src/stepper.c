#include <stdint.h>
#include <stdio.h>
#include <stdbool.h>

#include "pico/stdlib.h"
#include "dist.h"
#include "hardware/pwm.h"

#define U_STEPS 16
#define STATES_PER_ROT 200*U_STEPS

// Delay in ms, please note that changing it may affect distance being traversed by gear
// It happens due to poor documentation, and minimum time between switchin positions not being specified

#define US_DELAY_PER_STATE  3000

#define CLOCKWISE           0
#define COUNTER_CLOCKWISE   1
#define MAX_DIR             1

#define STEPPER_COUNT       2

#define DIR_0_PIN           0
#define STEP_0_PIN          1
#define EN_0_PIN            2

#define DIR_1_PIN           3
#define STEP_1_PIN          4
#define EN_1_PIN            5

#define SIZE_X              700                
#define SIZE_Y              700                

typedef struct stepper_ctx_t {
  uint8_t step_pin;
  uint8_t dir_pin;
  uint8_t en_pin;
} stepper_ctx_t;

stepper_ctx_t steppers[2] = {
  {
    .step_pin   = STEP_0_PIN,
    .dir_pin    = DIR_0_PIN,
    .en_pin     = EN_0_PIN,
  }
  ,
    {
      .step_pin   = STEP_1_PIN,
      .dir_pin    = DIR_1_PIN,
      .en_pin     = EN_1_PIN,
    }
};

int cur_x = 0;
int cur_y = 0;

int get_x(void)
{
  return cur_x;
}

int get_y(void)
{
  return cur_y;
}

  static void
step(int count)
{
  for(int i=0; i<count*2; i++) {
    gpio_put(steppers[0].step_pin, i%2);
    gpio_put(steppers[1].step_pin, i%2);
    sleep_us(US_DELAY_PER_STATE);
  }
}
  static inline void
set_en(bool en, stepper_ctx_t *stepper_ctx)
{
  gpio_put(stepper_ctx->en_pin, en);
}

  static inline void
set_dir(uint8_t dir, stepper_ctx_t *stepper_ctx)
{
  if (dir > MAX_DIR) {
    return;
  }

  gpio_put(stepper_ctx->dir_pin, dir);
}

  static inline void
maybe_move_y(int y)
{
  if(y == 0)
    return;

  if(y < 0){
    set_dir(COUNTER_CLOCKWISE, &steppers[0]);
    set_dir(CLOCKWISE, &steppers[1]);
    set_en(true, &steppers[0]);
    set_en(true, &steppers[1]);

    y = 0 - y;
    step(y);
    return;
  }

  set_dir(CLOCKWISE, &steppers[0]);
  set_dir(COUNTER_CLOCKWISE, &steppers[1]);
  set_en(true, &steppers[0]);
  set_en(true, &steppers[1]);

  step(y);

  set_en(false, &steppers[0]);
  set_en(false, &steppers[1]);
}

  static void
move_x(int x)
{
  if(x == 0)
    return;

  if(x < 0){
    set_dir(CLOCKWISE, &steppers[0]);
    set_dir(CLOCKWISE, &steppers[1]);
    set_en(true, &steppers[0]);
    set_en(true, &steppers[1]);

    x = 0 - x;
    step(x);
    return;
  }

  set_dir(COUNTER_CLOCKWISE, &steppers[0]);
  set_dir(COUNTER_CLOCKWISE, &steppers[1]);
  set_en(true, &steppers[0]);
  set_en(true, &steppers[1]);

  step(x);

  set_en(false, &steppers[0]);
  set_en(false, &steppers[1]);
}


  static inline void
move_diag(int *x, int *y)
{
  int cur_step, no_step;
  int cur_rot;
  int dist;

  const int x_abs = *x < 0 ? 0 - *x : *x;
  const int y_abs = *y < 0 ? 0 - *y : *y;

  const int diff = x_abs < y_abs ? x_abs : y_abs;

  if(*y < 0)
  {
    cur_step = 1;
    no_step = 0;
  }
  else
  {
    cur_step = 0;
    no_step = 1;
  }

  if(*x < 0)
    cur_rot = CLOCKWISE;
  else
    cur_rot = COUNTER_CLOCKWISE;

  set_en(false, &steppers[no_step]);
  set_en(true, &steppers[cur_step]);

  set_dir(cur_rot, &steppers[cur_step]);

  step(diff);

  *x = *x < 0 ? diff - x_abs : x_abs - diff;
  *y = *y < 0 ? diff - y_abs : y_abs - diff;
}

  void
setup_gpio(void)
{
#pragma unroll
  for(int i=0; i<STEPPER_COUNT; i++){
    gpio_init(steppers[i].step_pin);
    gpio_init(steppers[i].dir_pin);
    gpio_init(steppers[i].en_pin);

    gpio_set_dir(steppers[i].step_pin, GPIO_OUT);
    gpio_set_dir(steppers[i].dir_pin , GPIO_OUT);
    gpio_set_dir(steppers[i].en_pin , GPIO_OUT);

    gpio_put(steppers[i].en_pin, 0);
  }
}

  void
move_start(void)
{
  for (int i=0; i<20; i++) {
    set_en(true, &steppers[1]);

    move_x(-40);
    maybe_move_y(-50);

    set_en(false, &steppers[1]);

    const uint32_t dist = get_dist();
    if (dist <= 130) {
      break;
    }
  }

  move_x(30);
  maybe_move_y(30);

  move_x(-30);
  maybe_move_y(-30);

  cur_x = 0;
  cur_y = 0;

  set_en(false, &steppers[0]);
  set_en(false, &steppers[1]);
}

  void
move(int x_pos, int y_pos)
{
  int x = x_pos - cur_x;
  int y = y_pos - cur_y;

  cur_x = x_pos;
  cur_y = y_pos;

  maybe_move_y(y);
  move_x(x);

  set_en(false, &steppers[0]);
  set_en(false, &steppers[1]);
}
