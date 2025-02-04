#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdlib.h>

#include <linux/i2c-dev.h>
#include <linux/i2c.h>

#include <i2c/smbus.h>

#include <sys/ioctl.h>

#define BUFFER_SIZE 64

#define VSYNC   3
#define HSYNC   4
#define PCLK    5
#define DATAEN  6

int ti_i2c_init(void);
const int ti_i2c_sanity_check(void);
void ti_i2c_deinit(void);
const int ti_i2c_init_screen(void);
const int ti_i2c_curtain_on(void);
const int ti_i2c_curtain_off(void);
