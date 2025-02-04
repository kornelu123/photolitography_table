#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>

#include <linux/i2c-dev.h>
#include <linux/i2c.h>

#include <i2c/smbus.h>

#include <sys/ioctl.h>

#include "ti_i2c.h"

#define I2C_MAX_DATA 0x20

const char* i2c_dev = "/dev/i2c-2";
const uint8_t dev_addr = 0x1b;
int i2c_fd;

uint8_t buffer[BUFFER_SIZE];

struct i2c_rdwr_ioctl_data *i2c_data;

    static inline uint32_t
bytes_to_uint32(uint8_t *data)
{
    return data[0] << 24 | data[1] << 16 | data[2] << 8 | data[3];
}

    static inline void
uint32_to_bytes(uint32_t in, uint8_t *out)
{
    out[0] = in & 0xFF000000;
    out[1] = in & 0x00FF0000;
    out[2] = in & 0x0000FF00;
    out[3] = in & 0x000000FF;
}

    int
ti_i2c_init(void)
{
    i2c_fd = open(i2c_dev, O_RDWR);

    if(i2c_fd < 0)
    {
        perror("Error getting fd");
        return i2c_fd;
    }

    if (ioctl(i2c_fd, I2C_SLAVE, dev_addr) < 0) {
        perror("I2C_SLAVE failed");
        return -1;
    }

    return 0;
}

    void
ti_i2c_deinit(void)
{
    close(i2c_fd);
}

    const inline int
ti_i2c_read(uint8_t *data, uint8_t data_size, uint8_t addr)
{
    uint8_t subaddr[2] = {0x15, addr};

    if (write(i2c_fd, subaddr, 2) != 2) {
        perror("write");
        return -1;
    }

    if (read(i2c_fd, data, data_size) < 0) {
        perror("read");
        return -1;
    }

    return 0;
}

    const inline int
ti_i2c_write(uint8_t *data, uint8_t data_size, uint8_t addr)
{
    if (data_size > 4 || data_size < 1) {
        fprintf(stderr, "Too long i2c transaction\n");
        return -1;
    }

    uint8_t data_b[5];
    data_b[0] = addr;

    for (int i=0; i<data_size; i++) {
        data_b[i+1] = data[i];
    }

    if (write(i2c_fd, data_b, data_size + 1) != data_size + 1) {
        perror("write");
        return -1;
    }

    return 0;
}

    const int
ti_i2c_curtain_on(void)
{
    uint8_t buf[4];

    uint32_to_bytes(0x00000001, buf);

    if (ti_i2c_write(buf, 0x04, 0xA6) < 0) {
        fprintf(stderr, "Setting input source failed\n");
        fflush(stderr);
        return -1;
    }
}

    const int 
ti_i2c_curtain_off(void)
{
    uint8_t buf[4];

    uint32_to_bytes(0x00000000, buf);

    if (ti_i2c_write(buf, 0x04, 0xA6) < 0) {
        fprintf(stderr, "Setting input source failed\n");
        fflush(stderr);
        return -1;
    }
}

    const int
ti_i2c_init_screen(void)
{
    // TODO read config from file, or rather move to lcdc driver
    ti_i2c_curtain_on();
    uint8_t buf[4];

    uint32_to_bytes(0x00000000, buf);

    // Setting parallel input as a input source
    if (ti_i2c_write(buf, 0x04, 0x0B) < 0) {
        fprintf(stderr, "Setting input source failed\n");
        fflush(stderr);
        return -1;
    }

    if (ti_i2c_read(buf, 0x04, 0x03) < 0) {
        return -1;
    }

    // Sanity check basically checks the device ID in main status register
    if (buf[3] != 0x8A) {
        fprintf(stderr, "Sanity check failed\n");
        fflush(stderr);
        return -1;
    }

    uint32_to_bytes(0x00000001, buf);

    // Setting RGB888 as input format
    if (ti_i2c_write(buf, 0x04, 0xA3) < 0) {
        fprintf(stderr, "Setting input format failed\n");
        fflush(stderr);
        return -1;
    }

    uint32_to_bytes(0x00000002, buf);

    // Setting RGB888 as input format
    if (ti_i2c_write(buf, 0x04, 0x0D) < 0) {
        fprintf(stderr, "Setting input format failed\n");
        fflush(stderr);
        return -1;
    }

    uint32_to_bytes(0x00000000, buf);

    // Set Parallel bus polarity control HSYNC/VSYNC
    if (ti_i2c_write(buf, 0x04, 0xA6) < 0) {
        fprintf(stderr, "Setting input format failed\n");
        fflush(stderr);
        return -1;
    }

    uint32_to_bytes(0x0000001B, buf);

    // Set landscape nHD resolution
    if (ti_i2c_write(buf, 0x04, 0x0C) < 0) {
        fprintf(stderr, "Setting input format failed\n");
        fflush(stderr);
        return -1;
    }

    uint32_to_bytes(0x00000000, buf);

    // Setting RGB888 as input format
    if (ti_i2c_write(buf, 0x04, 0xA3) < 0) {
        fprintf(stderr, "Setting input format failed\n");
        fflush(stderr);
        return -1;
    }

    uint32_to_bytes(0x000004FC, buf);

    // Setting RGB888 as input format
    if (ti_i2c_write(buf, 0x04, 0x19) < 0) {
        fprintf(stderr, "Setting input format failed\n");
        fflush(stderr);
        return -1;
    }

    uint32_to_bytes(0x00000001, buf);

    // Locking sequence to VSYNC
    if (ti_i2c_write(buf, 0x04, 0x1E) < 0) {
        fprintf(stderr, "Setting input format failed\n");
        fflush(stderr);
        return -1;
    }

    uint32_to_bytes(0x00000000, buf);

    // Setting parallel input as a input source
    if (ti_i2c_write(buf, 0x04, 0x0B) < 0) {
        fprintf(stderr, "Setting input source failed\n");
        fflush(stderr);
        return -1;
    }

    ti_i2c_curtain_off();

    return 0;
}
