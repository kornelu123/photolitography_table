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
#define DELAY_PER_STATE     9

#define MAX_MSG_LEN         16

#if DELAY_PER_STATE < 9
  #error "DELAY_PER_STATE should be at least 10"
#endif

#define CLOCKWISE           0
#define COUNTER_CLOCKWISE   1

typedef struct {
  uint32_t *cur_dir_masks;
  uint8_t   cur_step_ind;
  uint8_t   gpio_offset;
} stepper_ctx_t;

static const uint32_t gpio_stepper_masks[2][STATES_PER_STEP] = {
  {
    0xFFFFFFFF & 0x01,
    0xFFFFFFFF & 0x05,
    0xFFFFFFFF & 0x04,
    0xFFFFFFFF & 0x06,
    0xFFFFFFFF & 0x02,
    0xFFFFFFFF & 0x0a,
    0xFFFFFFFF & 0x08,
    0xFFFFFFFF & 0x09
  },{
    0xFFFFFFFF & 0x09,
    0xFFFFFFFF & 0x08,
    0xFFFFFFFF & 0x0a,
    0xFFFFFFFF & 0x02,
    0xFFFFFFFF & 0x06,
    0xFFFFFFFF & 0x04,
    0xFFFFFFFF & 0x05,
    0xFFFFFFFF & 0x01
  }
};

stepper_ctx_t stepper_0_ctx = {
  (uint32_t *)gpio_stepper_masks[CLOCKWISE],
  0,
  0
};

stepper_ctx_t stepper_1_ctx = {
  (uint32_t *)gpio_stepper_masks[CLOCKWISE],
  0,
  4
};

  void
setup_gpio(void)
{
#pragma unroll(8)
  for(int i=IN_A_0; i<=IN_NOT_B_1; i++){
    gpio_init(i);
    gpio_set_dir(i, GPIO_OUT);
  }
}

  void
set_dir(uint8_t dir, stepper_ctx_t *stepper_ctx)
{
  if(dir > COUNTER_CLOCKWISE)
    return;

  stepper_ctx->cur_step_ind = STATES_PER_STEP - 1 - stepper_ctx->cur_step_ind;

  if(dir == CLOCKWISE){
    stepper_ctx->cur_dir_masks = (uint32_t *)&gpio_stepper_masks[CLOCKWISE];
    return;
  }

  stepper_ctx->cur_dir_masks = (uint32_t *)&gpio_stepper_masks[COUNTER_CLOCKWISE];
}

  void
step(stepper_ctx_t *stepper_ctx)
{
  gpio_set_mask(stepper_ctx->cur_dir_masks[stepper_ctx->cur_step_ind]<< stepper_ctx->gpio_offset);
  sleep_ms(DELAY_PER_STATE);
  gpio_clr_mask(stepper_ctx->cur_dir_masks[stepper_ctx->cur_step_ind] << stepper_ctx->gpio_offset);
  stepper_ctx->cur_step_ind = (stepper_ctx->cur_step_ind + 1) % 8;
}

  void
maybe_move_y(int y)
{
  if(y == 0)
    return;

  if(y < 0){
    set_dir(CLOCKWISE, &stepper_0_ctx);
    set_dir(COUNTER_CLOCKWISE, &stepper_1_ctx);
    y = 0 - y;

    for(int i=0; i<y; i++){
      step(&stepper_0_ctx);
      step(&stepper_1_ctx);
    }
    return;
  }

  set_dir(COUNTER_CLOCKWISE, &stepper_0_ctx);
  set_dir(CLOCKWISE, &stepper_1_ctx);

  for(int i=0; i<y; i++){
    step(&stepper_0_ctx);
    step(&stepper_1_ctx);
  }
}

  void
move(int x, int y)
{
  if(x == 0)
    return maybe_move_y(y);

  if(x < 0){
    set_dir(COUNTER_CLOCKWISE, &stepper_0_ctx);
    set_dir(COUNTER_CLOCKWISE, &stepper_1_ctx);
    x = 0 - x;

    for(int i=0; i<x; i++){
      step(&stepper_0_ctx);
      step(&stepper_1_ctx);
    }
    return maybe_move_y(y);;
  }

  set_dir(CLOCKWISE, &stepper_0_ctx);
  set_dir(CLOCKWISE, &stepper_1_ctx);

  for(int i=0; i<x; i++){
    step(&stepper_0_ctx);
    step(&stepper_1_ctx);
  }
  return maybe_move_y(y);
}

  int
main(void)
{
  stdio_init_all();
  setup_gpio();

  while(true){
    char msg[MAX_MSG_LEN];
    int x, y;

    int count = 0;

    char c;
    do{
      c = getchar();
      msg[count] = c;
      count++;
      printf("%c", c);
    }while(count < MAX_MSG_LEN && c != '\r');


    if(sscanf(msg,"%d %d", &x, &y) != 2){
      printf("scanf Usage : <x> <y>\n x,yE <-255:255>\n");
      continue;
    }

    if(x < -255 || y < -255 || x > 255 || y > 255){
      printf("Usage : <x> <y>\n x,yE <-255:255>\n");
      continue;
    }

    move(x,y);
  }

  return 0;
}
