#define LCD_REGISTERS_ADDR 0x4830E000
#define LCD_CLEAR_IRQ      0x5C
#define LCD_READ_IRQ       0x58
#define LCD_REGISTERS_SIZE 0x1000

#define LCD_WRITE _IOW('k', 1, struct lcd_ioctl_data)
#define LCD_READ  _IOR('k', 2, struct lcd_ioctl_data)
#define FBIGET_BRIGHTNESS   _IOR('F', 3, int)
#define FBIPUT_BRIGHTNESS   _IOW('F', 4, int)
#define FBIGET_COLOR        _IOR('F', 5, int)
#define FBIPUT_COLOR        _IOW('F', 6, int)
#define FBIPUT_HSYNC        _IOW('F', 9, int)
#define FBIPUT_VSYNC        _IOW('F', 10, int)
#define FB_BLACKOUT     _IO('F', 11)
#define FB_RESTORE      _IO('F', 12)
#define FB_OFF          _IO('F', 13)
#define FB_ON           _IO('F', 14)
#define FB_RESET        _IO('F', 15)
#define FBIOGET_CONTRAST    _IOR('F', 16, int)
#define FBIOPUT_CONTRAST    _IOW('F', 17, int)

struct lcd_ioctl_data {
    unsigned int address;
    unsigned int data;
};
