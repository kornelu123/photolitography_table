#include <stdint.h>
#include <stdio.h>
#include <errno.h>
#include <gpiod.h>
#include "ti_gpio.h"

struct gpiod_chip *gpio_chip[4];
struct gpiod_line *ctrl_gpio[CTRL_GPIO_COUNT];

const gpiod_init_info init_info[CTRL_GPIO_COUNT] = {
    {"gpiochip0", 16, GPIOD_LINE_DIRECTION_OUTPUT_},  // PROJ_ON_EXT
    {"gpiochip0", 29, GPIOD_LINE_DIRECTION_INPUT_},   // GPIO_INIT_DONE
    {"gpiochip1", 1,  GPIOD_LINE_DIRECTION_INPUT_}
};

int ti_gpio_init(void) {
    for (int i = 0; i < 4; i++) {
        char fname[16];
        sprintf(fname, "/dev/gpiochip%d", i);
        gpio_chip[i] = gpiod_chip_open(fname);
        if (!gpio_chip[i]) {
            perror("Open chip failed");
            return -1;
        }
    }
    return 0;
}

void ti_gpio_deinit(void) {
    for (int i = 0; i < CTRL_GPIO_COUNT; i++) {
        if (ctrl_gpio[i]) {
            gpiod_line_release(ctrl_gpio[i]);
            ctrl_gpio[i] = NULL;
        }
    }
    for (int i = 0; i < 4; i++) {
        if (gpio_chip[i]) {
            gpiod_chip_close(gpio_chip[i]);
            gpio_chip[i] = NULL;
        }
    }
}

static inline struct gpiod_line*
ti_init_line(uint32_t line_id, const char *chip, enum gpiod_line_direction dir) {
    struct gpiod_line *line = gpiod_line_get(chip, line_id);
    if (!line) {
        perror("gpiod_line_get failed");
        return NULL;
    }

    int ret;
    if (dir == GPIOD_LINE_DIRECTION_OUTPUT_) {
        ret = gpiod_line_request_output(line, CONS_NAME, 0);
    } else {
        ret = gpiod_line_request_input(line, CONS_NAME);
    }

    if (ret < 0) {
        perror("gpiod_line_request failed");
        gpiod_line_release(line);
        return NULL;
    }

    return line;
}

int ti_init_ctrl_pins(void) {
    for (int i = 0; i < CTRL_GPIO_COUNT; i++) {
        ctrl_gpio[i] = ti_init_line(init_info[i].line, init_info[i].chip_name, init_info[i].dir);
        if (!ctrl_gpio[i]) {
            ti_gpio_deinit(); // Clean up on failure
            return -1;
        }
    }
    return 0;
}
