#include <time.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdint.h>

#include "log.h"

#define TABLE_MOVE_TIMEOUT_S 10

#define send_cmd(args...) \
fprintf(table_fd, args); \
fflush(table_fd); \
printf("[Printed to table]"); \
dbg_printf(args); \

FILE* table_fd;

    int
table_init(const char* rpi_path)
{
    table_fd = fopen(rpi_path, "w+");

    if (table_fd == NULL) {
        exit(-1);
    }

    return 0;
}

    int
move_table(int16_t x, int8_t y)
{
    send_cmd("%d %d \r", x, y);

    clock_t deadline = clock() + TABLE_MOVE_TIMEOUT_S*CLOCKS_PER_SEC;

    char data[32];
    
    while (clock() < deadline) {
        fgets(data, sizeof(char)*32, table_fd);

        if (strncmp(data, "Done", 4) == 0) {
            return 0;
        }

        fprintf(table_fd, "%d %d\r", x, y);
        fflush(table_fd);
    }

    fprintf(stderr, "Failed to receive table movement ack\n");
    return 0;
}

    int
reset_table_pos(void)
{
    send_cmd("start\r");

    clock_t deadline = clock() + TABLE_MOVE_TIMEOUT_S*CLOCKS_PER_SEC;

    char data[32];
    
    while (clock() < deadline) {
        fgets(data, sizeof(char)*32, table_fd);

        if (strncmp(data, "Done", 4) == 0) {
            return 0;
        }
    }

    return -1;
}
