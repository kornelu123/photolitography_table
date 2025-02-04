#define LCD_REGISTERS_ADDR 0x4830E000
#define LCD_CLEAR_IRQ      0x5C
#define LCD_READ_IRQ       0x58
#define LCD_REGISTERS_SIZE 0x1000

#define LCD_WRITE _IOW('k', 1, struct lcd_ioctl_data)
#define LCD_READ  _IOR('k', 2, struct lcd_ioctl_data)

struct lcd_ioctl_data {
    unsigned int address;
    unsigned int data;
};
