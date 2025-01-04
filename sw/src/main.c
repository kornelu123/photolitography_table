#include <stdint.h>
#include <stdio.h>

#include "pico/stdlib.h"

#include "stepper.c"

#define MAX_MSG_LEN         16

int
main(void)
{
  stdio_init_all();
  setup_gpio();

  gpio_init(PICO_DEFAULT_LED_PIN);
  gpio_set_dir(PICO_DEFAULT_LED_PIN, GPIO_OUT);
  gpio_put(PICO_DEFAULT_LED_PIN, true);

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
      printf("Usage : <x> <y>\n x,yE <-255:255>\n");
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
