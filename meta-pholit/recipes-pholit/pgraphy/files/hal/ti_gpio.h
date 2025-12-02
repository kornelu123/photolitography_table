#include <gpiod.h>
#include <stdint.h>

#define CTRL_GPIO_COUNT 3
#define RGB_GPIO_COUNT 24

#define PROJ_ON_EXT 0
#define GPIO_INIT_DONE 1

#define VSYNC 3
#define HSYNC 4
#define PCLK 5
#define DATAEN 6

#define HPIX 640
#define VPIX 360

#define CONS_NAME "pholit"

extern struct gpiod_chip *gpio_chip[4];
extern struct gpiod_line *ctrl_gpio[CTRL_GPIO_COUNT];

enum gpiod_line_direction {
    GPIOD_LINE_DIRECTION_INPUT_,
    GPIOD_LINE_DIRECTION_OUTPUT_
};

typedef struct gpiod_init_info{
    const char *chip_name;
    uint8_t line;
    enum gpiod_line_direction dir;
} gpiod_init_info;

int ti_gpio_init(void);
void ti_gpio_deinit(void);
int ti_init_ctrl_pins(void);
