#include <stdint.h>
#include <stdio.h>

#include "pico/stdlib.h"
#include "hardware/pwm.h"

#define IN_A_0        0
#define IN_NOT_A_0    1
#define IN_B_0        2
#define IN_NOT_B_0    3

#define IN_A_1        4
#define IN_NOT_A_1    5
#define IN_B_1        6
#define IN_NOT_B_1    7

#define STATES_PER_STEP     8
#define STEPS_PER_ROT       12
#define STATES_PER_ROT      STATES_PER_STEP*STEPS_PER_ROT

// Delay in ms, please note that changing it may affect distance being traversed by gear
// It happens due to poor documentation, and minimum time between switchin positions not being specified

#define DELAY_PER_STATE     6

#if DELAY_PER_STATE < 6
#error "DELAY_PER_STATE should be at least 9"
#endif

#define CLOCKWISE           0
#define COUNTER_CLOCKWISE   1
#define NO_ROT              2

#define STEPPER_COUNT       2

typedef struct {
  uint32_t *cur_dir_masks;
  uint8_t   cur_step_ind;
  uint8_t   gpio_offset;
} stepper_ctx_t;

static const uint32_t gpio_stepper_masks[3][STATES_PER_STEP] = {
  {
    0x00000001,
    0x00000005,
    0x00000004,
    0x00000006,
    0x00000002,
    0x0000000A,
    0x00000008,
    0x00000009
  },{
    0x00000009,
    0x00000008,
    0x0000000A,
    0x00000002,
    0x00000006,
    0x00000004,
    0x00000005,
    0x00000001
  },{
    0x00000000,
    0x00000000,
    0x00000000,
    0x00000000,
    0x00000000,
    0x00000000,
    0x00000000,
    0x00000000
  }
};

stepper_ctx_t stepper_ctx[2] = {
  {
    .cur_dir_masks = (uint32_t *)gpio_stepper_masks[CLOCKWISE],
    .cur_step_ind = 0,
    .gpio_offset = 0
  },{
    .cur_dir_masks = (uint32_t *)gpio_stepper_masks[CLOCKWISE],
    .cur_step_ind = 0,
    .gpio_offset = 4
  }
};

  static inline void
set_dir(uint8_t dir, stepper_ctx_t *stepper_ctx)
{
  if(dir > NO_ROT)
    return;

  stepper_ctx->cur_step_ind = STATES_PER_STEP - 1 - stepper_ctx->cur_step_ind;

  stepper_ctx->cur_dir_masks = (uint32_t *)&gpio_stepper_masks[dir];
}

// TOOD: change this so it moves both motors simultaneously
  static inline void
step(stepper_ctx_t stepper_ctx[])
{
  uint32_t mask = 0x00000000;

  for(int i=0; i<STEPPER_COUNT; i++)
  {
    mask |= (stepper_ctx[i].cur_dir_masks[stepper_ctx[i].cur_step_ind] << stepper_ctx[i].gpio_offset);
  }

  gpio_set_mask(mask);

  sleep_ms(DELAY_PER_STATE);

  gpio_clr_mask(mask);

  for(int i=0; i<STEPPER_COUNT; i++)
  {
    if(stepper_ctx[i].cur_dir_masks == gpio_stepper_masks[NO_ROT])
      continue;

    stepper_ctx[i].cur_step_ind = (stepper_ctx[i].cur_step_ind + 1) % 8;
  }
}

  static inline void
maybe_move_y(int y)
{
  if(y == 0)
    return;

  if(y < 0){
    set_dir(COUNTER_CLOCKWISE, &stepper_ctx[0]);
    set_dir(CLOCKWISE, &stepper_ctx[1]);
    y = 0 - y;

    for(int i=0; i<y; i++){
      step(stepper_ctx);
    }
    return;
  }

  set_dir(CLOCKWISE, &stepper_ctx[0]);
  set_dir(COUNTER_CLOCKWISE, &stepper_ctx[1]);

  for(int i=0; i<y; i++){
    step(stepper_ctx);
  }
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

  set_dir(cur_rot, &stepper_ctx[cur_step]);
  set_dir(NO_ROT, &stepper_ctx[no_step]);

  for(int i=0; i<diff; i++)
    step(stepper_ctx);

  *x = *x < 0 ? diff - x_abs : x_abs - diff;
  *y = *y < 0 ? diff - y_abs : y_abs - diff;
}

  void
setup_gpio(void)
{
#pragma unroll
  for(int i=0; i<STEPPER_COUNT*4; i++){
    gpio_init(i);
    gpio_set_dir(i, GPIO_OUT);
  }
}

  void
move(int x, int y)
{
  if(x && y)
    move_diag(&x, &y);

  if(x == 0)
    return maybe_move_y(y);

  if(x < 0){
    set_dir(CLOCKWISE, &stepper_ctx[0]);
    set_dir(CLOCKWISE, &stepper_ctx[1]);
    x = 0 - x;

    for(int i=0; i<x; i++){
      step(stepper_ctx);
    }

    return;
  }

  set_dir(COUNTER_CLOCKWISE, &stepper_ctx[0]);
  set_dir(COUNTER_CLOCKWISE, &stepper_ctx[1]);

  for(int i=0; i<x; i++){
    step(stepper_ctx);
  }
}
