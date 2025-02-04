#include "image.hpp"

extern "C" {
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>

#include <argp.h>

#include <gpiod.h>

#include "hal/ti_gpio.h"
#include "hal/ti_i2c.h"
#include "table.c"
#include "log.h"
}

#define SLEEP_MS(x) usleep(x*1000)

#define EVM2000_PROJ_ON_TIMEOUT_S 2

int fb_fd;

struct arguments {
    int xsize, ysize;
    int overlap;
    int xstep, ystep;
    int time;
    uint8_t brightness;

    char *file;
    char *rpi_path;
};

typedef struct pgraphy_ctx_t {
    cv::Mat main_img;

    struct arguments args;
} pgraphy_ctx_t;

pgraphy_ctx_t pgraphy_ctx;

bool debug = false;

    error_t
parse_opt(int key, char *arg, struct argp_state *state)
{
    struct arguments *arguments = (struct arguments *)state->input;
    int val;

    switch (key) {
        case 'b': 
            val = atoi(arg);
            if (val > 255 || val < 0) {
                return ARGP_ERR_UNKNOWN;
            }
            arguments->brightness = (uint8_t )val;
            break;
        case 'x': 
            arguments->xsize = atoi(arg);
            break;
        case 'y':
            arguments->ysize = atoi(arg);
            break;
        case 'o':
            arguments->overlap = atoi(arg);
            break;
        case 'w':
            arguments->xstep = atoi(arg);
            break;
        case 'h':
            arguments->ystep = atoi(arg);
            break;
        case 't':
            arguments->time = atoi(arg);
            break;
        case 'f':
            arguments->file = arg;
            break;
        case 'p':
            arguments->rpi_path = arg;
            break;
        case 'd':
            debug = true;
            break;
        default:
            return ARGP_ERR_UNKNOWN;
    }

    return 0;
}

    void
deinit_all(void)
{
    ti_i2c_deinit();
    ti_gpio_deinit();
    close(fb_fd);
}

    int
init_all(void)
{
    if(ti_gpio_init() < 0)
    {
        ti_gpio_deinit();
        return -1;
    }
    dbg_printf("Initialised gpio\n");

    if(ti_init_ctrl_pins() < 0)
        exit(-1);
    dbg_printf("Initialised ctrl gpio pins\n");

    if(ti_i2c_init() < 0)
        exit(-1);
    dbg_printf("Initialised i2c \n");

    gpiod_line_set_value(ctrl_gpio[PROJ_ON_EXT], 1);
    dbg_printf("Turning EVM2000 on via PROJ_ON_EXT pin\n");

    clock_t deadline = clock() + EVM2000_PROJ_ON_TIMEOUT_S*CLOCKS_PER_SEC;

    while(gpiod_line_get_value(ctrl_gpio[GPIO_INIT_DONE])) {
        if (clock() >= deadline) {
            printf("Initialisation of EVM2000 exceeded %d seconds deadline\n"); 
            exit(-1);
        }
    }

    ti_i2c_init_screen();
    dbg_printf("Intitialised EVM2000 via i2c\n");

    fb_fd = open("/dev/fb0", O_WRONLY);

    if (fb_fd == -1) {
        perror("Open failed:");
        return -1;
    }
    dbg_printf("Intitialised fb\n");

    if (table_init(pgraphy_ctx.args.rpi_path) != 0) {
        printf("table init failed");
        deinit_all();
        exit(-1);
    }
    dbg_printf("Initialised remote table rpi@%s\n", pgraphy_ctx.args.rpi_path);

    return 0;
}

#define SINGLE_IMG_WIDTH_UM     160
#define SINGLE_IMG_HEIGHT_UM    180

#define UM_TO_PX(x)             (x)

    void
__main(void)
{
    blackout_screen();

    reset_table_pos();

    cv::resize(pgraphy_ctx.main_img, pgraphy_ctx.main_img, cv::Size(WIDTH, HEIGHT));

    for (int x=WIDTH - SINGLE_IMG_WIDTH_UM; x>=0; x-=SINGLE_IMG_WIDTH_UM) {
        for (int y=0; y<HEIGHT; y+=SINGLE_IMG_HEIGHT_UM) {
            cv::Rect sub(x, y, SINGLE_IMG_WIDTH_UM, SINGLE_IMG_HEIGHT_UM);
            cv::Mat cur_part = pgraphy_ctx.main_img(sub);
            cv::Mat print_part;

            move_table((x/SINGLE_IMG_WIDTH_UM)*pgraphy_ctx.args.xstep, ((HEIGHT - y)/SINGLE_IMG_HEIGHT_UM)*pgraphy_ctx.args.ystep);

            SLEEP_MS(1000);

            cv::resize(cur_part, print_part, cv::Size(WIDTH, HEIGHT));

            cv::flip(print_part, print_part, 1);
            print_part = moveRightToLeft(print_part, 35);

            dbg_printf("Displaying X:[%u/%u], Y:[%u/%u] img part\n" ,
                       (WIDTH - x)/SINGLE_IMG_WIDTH_UM, WIDTH/SINGLE_IMG_WIDTH_UM,
                       (y)/SINGLE_IMG_HEIGHT_UM + 1, HEIGHT/SINGLE_IMG_HEIGHT_UM);
            write_img(print_part.data, 640*360*3);
            SLEEP_MS(pgraphy_ctx.args.time);

            blackout_screen();
        }
    }
}

struct argp_option options[] = {
    { "xsize", 'x', "XSIZE", 0, "Width of projected image (in um)." },
    { "ysize", 'y', "YSIZE", 0, "Height of projected image (in um)." },
    { "overlap", 'o', "OVRLP", 0, "Overlap of parts of image (in um)." },
    { "stepx", 'w', "STEP", 0, "Width of one step (in um)." },
    { "stepy", 'h', "STEP", 0, "Height of one step (in um)." },
    { "time", 't', "TIME", 0, "Time of exposure (in ms)." },
    { "file", 'f', "FILE", 0, "Path to file (required)." },
    { "debug", 'd', 0, 0, "Enable debug mode" },
    { "brightness", 'b', "BRIGHTNESS", 0, "Adjust brightness <0;255> [Default 255]" }, 
    { "rpi_path", 'p', "PATH", 0, "Path to RPi pico" },
    { 0 }
};

const char *argp_program_version = "pgraphy ver 1.0.0";
const char *argp_program_bug_addr = "koruddl@gmail.com";
char doc[] = "Program for exeucting photolitography process from image";
char args_doc[] = " [OPTIONS]";

struct argp argp = { options, parse_opt, args_doc, doc };

    int
main(int argc, char **argv)
{
    // TODO: read those default values from config file
    pgraphy_ctx.args.xsize = 640;
    pgraphy_ctx.args.ysize = 360;
    pgraphy_ctx.args.overlap = 0;
    pgraphy_ctx.args.xstep = 50;
    pgraphy_ctx.args.ystep = 50;
    pgraphy_ctx.args.time = 1000;
    pgraphy_ctx.args.brightness = 255;
    pgraphy_ctx.args.file = NULL;

    argp_parse(&argp, argc, argv, 0, 0, &(pgraphy_ctx.args));

    if (pgraphy_ctx.args.file == NULL) {
        fprintf(stderr, "Error: The -f argument is mandatory\n");
        argp_help(&argp, stderr, ARGP_HELP_STD_USAGE, argv[0]);
        exit(-1);
    }

    dbg_printf("Running with parameters: \n" 
            "xsize, ysize: \t\t\t %u, %u\n"
            "xstep, ystep: \t\t\t %u, %u\n"
            "overlap: \t\t\t %u\n"
            "time: \t\t\t\t %u\n"
            "brightness: \t\t\t %u\n"
            "file: \t\t\t\t %s\n"
            "path to rpi: \t\t\t %s\n",
            pgraphy_ctx.args.xsize, pgraphy_ctx.args.ysize,
            pgraphy_ctx.args.xstep, pgraphy_ctx.args.ystep,
            pgraphy_ctx.args.overlap,
            pgraphy_ctx.args.time,
            pgraphy_ctx.args.brightness,
            pgraphy_ctx.args.file,
            pgraphy_ctx.args.rpi_path);


    if (init_all() != 0) {
        fprintf(stderr, "init all failed");
        exit(-1);
    }
    dbg_printf("Initialised all succesfully\n");

    pgraphy_ctx.main_img = read_img(pgraphy_ctx.args.file, pgraphy_ctx.args.brightness);
    dbg_printf("Image read : %s\n", pgraphy_ctx.args.file);

    __main();

    deinit_all();

    return 0;
}
