#include <stdint.h>
#include <stdio.h>

#include "dist.h"

volatile uint32_t dist;
auto_init_mutex(dist_mut);

  void
init_dist(void)
{
  gpio_init(TRIG_PIN);
  gpio_init(ECHO_PIN);

  gpio_set_dir(TRIG_PIN, GPIO_OUT);
  gpio_set_dir(ECHO_PIN, GPIO_IN);

  gpio_put(TRIG_PIN, 0);
}

  void
get_us_dist(void)
{
  gpio_put(TRIG_PIN, 1);
  sleep_ms(1000);
  gpio_put(TRIG_PIN, 0);

  absolute_time_t deadline = make_timeout_time_ms(READ_TIMEOUT_MS);

  while(!gpio_get(ECHO_PIN)) {
    if (deadline < get_absolute_time())
      mutex_enter_blocking(&dist_mut);
      dist = -1;
      mutex_exit(&dist_mut);
  }

  absolute_time_t time_start = get_absolute_time();

  deadline = make_timeout_time_ms(READ_TIMEOUT_MS);

  while(gpio_get(ECHO_PIN)) {
    if (deadline < get_absolute_time())
      mutex_enter_blocking(&dist_mut);
      dist = -1;
      mutex_exit(&dist_mut);
  }

  absolute_time_t time_end = get_absolute_time();

  mutex_enter_blocking(&dist_mut);
  dist = (absolute_time_diff_us(time_start, time_end));
  mutex_exit(&dist_mut);
}

  void
dist_work(void)
{
  init_dist();

  while (0xDEADBEEF) {
    get_us_dist();
    sleep_ms(300);
  }
}

  uint32_t
get_dist(void)
{
  uint32_t ret;

  mutex_enter_blocking(&dist_mut);
  ret = dist;
  mutex_exit(&dist_mut);

  return ret;
}
