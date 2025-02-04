#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "pico/stdlib.h"

#include "dist.h"
#include "stepper.c"

#define USAGE_PRINT         "Usage : <x> <y>\n xE <0, %u>, yE <0, %u>\n", SIZE_X, SIZE_Y
#define MAX_MSG_LEN         16

#if CONFIG_CMD == CONFIG_CONST_UART
  void get_msg(char *msg) {
    int count = 0;

    char c;
    do{
      c = getchar();
      msg[count] = c;
      count++;
    }while(count < MAX_MSG_LEN && c != '\r');
  }

  void ret_msg(const char *msg, ...) {
    va_list args;
    va_start(args, msg);

    vprintf(msg, args);

    va_end(args);
  }
#endif

  void
__main(void)
{
  while(true){
    char msg[MAX_MSG_LEN];
    int x, y;

    get_msg(msg);

    if(strncmp(msg, "start", 5) == 0) {
      move_start();
      ret_msg("Done\n");
      continue;
    }

    if(strncmp(msg, "get_pos", 7) == 0) {
      ret_msg("%d %d", get_x(), get_y());
      continue;
    }

    if(sscanf(msg,"%d %d", &x, &y) != 2){
      continue;
    }

    if(x > SIZE_X || y > SIZE_Y || x < 0 || y < 0) {
      continue;
    }

    move(x,y);
    ret_msg("Done\n");
  }
}

  int
main(void)
{
  stdio_init_all();
  setup_gpio();

  gpio_init(PICO_DEFAULT_LED_PIN);
  gpio_set_dir(PICO_DEFAULT_LED_PIN, GPIO_OUT);
  gpio_put(PICO_DEFAULT_LED_PIN, true);

  multicore_launch_core1(dist_work);

  __main();

  return 0;
}
