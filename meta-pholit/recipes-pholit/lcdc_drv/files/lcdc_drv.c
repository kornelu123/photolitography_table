#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fb.h>
#include <linux/dma-mapping.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/uaccess.h>
#include <linux/pm_runtime.h>
#include <linux/suspend.h>
#include <linux/interrupt.h>
#include <linux/wait.h>
#include <linux/clk.h>
#include <linux/cpufreq.h>
#include <linux/console.h>
#include <linux/spinlock.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/lcm.h>
#include <linux/pinctrl/consumer.h>
#include <video/of_display_timing.h>

#include "lcdc_drv.h"

#define DRIVER_NAME "lcdc_evm2000"

#define LCD_VERSION_1	1
#define LCD_VERSION_2	2

/* LCD Status Register */
#define LCD_END_OF_FRAME1		BIT(9)
#define LCD_END_OF_FRAME0		BIT(8)
#define LCD_PL_LOAD_DONE		BIT(6)
#define LCD_FIFO_UNDERFLOW		BIT(5)
#define LCD_SYNC_LOST			BIT(2)
#define LCD_FRAME_DONE			BIT(0)

/* LCD DMA Control Register */
#define LCD_DMA_BURST_SIZE(x)		((x) << 4)
#define LCD_DMA_BURST_1			0x0
#define LCD_DMA_BURST_2			0x1
#define LCD_DMA_BURST_4			0x2
#define LCD_DMA_BURST_8			0x3
#define LCD_DMA_BURST_16		0x4
#define LCD_V1_END_OF_FRAME_INT_ENA	BIT(2)
#define LCD_V2_END_OF_FRAME0_INT_ENA	BIT(8)
#define LCD_V2_END_OF_FRAME1_INT_ENA	BIT(9)
#define LCD_DUAL_FRAME_BUFFER_ENABLE	BIT(0)

/* LCD Control Register */
#define LCD_CLK_DIVISOR(x)		((x) << 8)
#define LCD_RASTER_MODE			0x01

/* LCD Raster Control Register */
#define LCD_PALETTE_LOAD_MODE(x)	((x) << 20)
#define PALETTE_AND_DATA		0x00
#define PALETTE_ONLY			0x01
#define DATA_ONLY			0x02

#define LCD_MONO_8BIT_MODE		BIT(9)
#define LCD_RASTER_ORDER		BIT(8)
#define LCD_TFT_MODE			BIT(7)
#define LCD_V1_UNDERFLOW_INT_ENA	BIT(6)
#define LCD_V2_UNDERFLOW_INT_ENA	BIT(5)
#define LCD_V1_PL_INT_ENA		BIT(4)
#define LCD_V2_PL_INT_ENA		BIT(6)
#define LCD_MONOCHROME_MODE		BIT(1)
#define LCD_RASTER_ENABLE		BIT(0)
#define LCD_TFT_ALT_ENABLE		BIT(23)
#define LCD_STN_565_ENABLE		BIT(24)
#define LCD_V2_DMA_CLK_EN		BIT(2)
#define LCD_V2_LIDD_CLK_EN		BIT(1)
#define LCD_V2_CORE_CLK_EN		BIT(0)
#define LCD_V2_LPP_B10			26
#define LCD_V2_TFT_24BPP_MODE		BIT(25)
#define LCD_V2_TFT_24BPP_UNPACK		BIT(26)

/* LCD Raster Timing 2 Register */
#define LCD_AC_BIAS_TRANSITIONS_PER_INT(x)	((x) << 16)
#define LCD_AC_BIAS_FREQUENCY(x)		((x) << 8)
#define LCD_SYNC_CTRL				BIT(25)
#define LCD_SYNC_EDGE				BIT(24)
#define LCD_INVERT_PIXEL_CLOCK			BIT(22)
#define LCD_INVERT_LINE_CLOCK			BIT(21)
#define LCD_INVERT_FRAME_CLOCK			BIT(20)

/* LCD Block */
#define  LCD_PID_REG				0x0
#define  LCD_CTRL_REG				0x4
#define  LCD_STAT_REG				0x8
#define  LCD_RASTER_CTRL_REG			0x28
#define  LCD_RASTER_TIMING_0_REG		0x2C
#define  LCD_RASTER_TIMING_1_REG		0x30
#define  LCD_RASTER_TIMING_2_REG		0x34
#define  LCD_DMA_CTRL_REG			0x40
#define  LCD_DMA_FRM_BUF_BASE_ADDR_0_REG	0x44
#define  LCD_DMA_FRM_BUF_CEILING_ADDR_0_REG	0x48
#define  LCD_DMA_FRM_BUF_BASE_ADDR_1_REG	0x4C
#define  LCD_DMA_FRM_BUF_CEILING_ADDR_1_REG	0x50

/* Interrupt Registers available only in Version 2 */
#define  LCD_RAW_STAT_REG			0x58
#define  LCD_MASKED_STAT_REG			0x5c
#define  LCD_INT_ENABLE_SET_REG			0x60
#define  LCD_INT_ENABLE_CLR_REG			0x64
#define  LCD_END_OF_INT_IND_REG			0x68

/* Clock registers available only on Version 2 */
#define  LCD_CLK_ENABLE_REG			0x6c
#define  LCD_CLK_RESET_REG			0x70
#define  LCD_CLK_MAIN_RESET			BIT(3)

#define LCD_NUM_BUFFERS	2

#define WSI_TIMEOUT	50
#define PALETTE_SIZE	256

#define	CLK_MIN_DIV	2
#define	CLK_MAX_DIV	255

#define DEBUG 1
#ifdef DEBUG
    #define DEBUG_PRINTF(fmt, ...) pr_info("[DEBUG] " fmt, ##__VA_ARGS__)
#else
    #define DEBUG_PRINTF(fmt, ...) // Expands to nothing in non-debug builds
#endif

typedef void (*vsync_callback_t)(void *arg);

enum panel_shade {
    MONOCHROME = 0,
    COLOR_ACTIVE,
    COLOR_PASSIVE,
};

enum raster_load_mode {
    LOAD_DATA = 1,
    LOAD_PALETTE,
};

enum lcdc_frame_complete {
    LCDC_FRAME_WAIT,
    LCDC_FRAME_NOWAIT,
};

struct lcd_ctrl_config {
    enum panel_shade panel_shade;

    int ac_bias;
    int ac_bias_intrpt;

    int dma_burst_sz;

    int bpp;
    int fdd;

    unsigned char tft_alt_mode;
    unsigned char stn_565_mode;
    unsigned char mono_8bit_mode;

    unsigned char sync_edge;
    unsigned char raster_order;

    int fifo_th;
};

struct lcd_sync_arg {
    int back_porch;
    int front_porch;
    int pulse_width;;
};

#define FBIOGET_CONTRAST    _IOR('F', 1, int)
#define FBIOPUT_CONTRAST    _IOW('F', 2, int)
#define FBIGET_BRIGHTNESS   _IOR('F', 3, int)
#define FBIPUT_BRIGHTNESS   _IOW('F', 3, int)
#define FBIGET_COLOR        _IOR('F', 5, int)
#define FBIPUT_COLOR        _IOW('F', 6, int)
#define FBIPUT_HSYNC        _IOW('F', 9, int)
#define FBIPUT_VSYNC        _IOW('F', 10, int)

struct lcdc_encoder {
    struct list_head list;
    struct i2c_client *client;

    void *priv;

    void (*set_mode) (struct lcdc_encoder *encoder, struct fb_videomode *panel);

    struct device_node *node;
};

struct lcdc_fb_data {
     struct device       *dev;
     resource_size_t p_palette_base;
     unsigned char *v_palette_base;
     dma_addr_t      vram_phys;
     unsigned long       vram_size;
     void            *vram_virt;
     unsigned int        dma_start;
     unsigned int        dma_end;
     struct clk *lcdc_clk;
     int irq;
     unsigned int palette_sz;
     int blank;
     wait_queue_head_t   vsync_wait;
     int         vsync_flag;
     int         vsync_timeout;
     spinlock_t      lock_for_chan_update;

     wait_queue_head_t   palette_wait;
     int         palette_loaded_flag;

     unsigned int        which_dma_channel_done;
 #ifdef CONFIG_CPU_FREQ
     struct notifier_block   freq_transition;
 #endif
     unsigned int        lcdc_clk_rate;
     void (*panel_power_ctrl)(int);
     u32 pseudo_palette[16];
     struct fb_videomode mode;
     struct lcd_ctrl_config  cfg;
     struct device_node *hdmi_node;
};

struct lcdc_platform_data {
    const char manu_name[10];
    void *controller_data;
    const char type[25];
    void (*panel_power_ctrl) (int);
};

static struct fb_videomode lcd_panels[] = {
    [0] = {
        .name               = "EVM2000",
        .xres               = 360,
        .yres               = 640,
        .pixclock           = (330000),
        .left_margin        = 4,
        .right_margin       = 8,
        .upper_margin       = 2,
        .lower_margin       = 1,
        .hsync_len          = 8,
        .vsync_len          = 1,
        .sync               = FB_SYNC_HOR_HIGH_ACT | FB_SYNC_VERT_HIGH_ACT,
    },
};

static struct fb_var_screeninfo lcdc_fb_var;

static struct fb_fix_screeninfo lcdc_fb_fix = {
    .id         = "EVM2000 FB Drv",
    .type       = FB_TYPE_PACKED_PIXELS,
    .type_aux   = 0,
    .visual     = FB_VISUAL_PSEUDOCOLOR,
    .xpanstep   = 0,
    .ypanstep   = 1,
    .ywrapstep  = 0,
    .accel      = FB_ACCEL_NONE
};

static vsync_callback_t vsync_cb_handler;
static void *vsync_cb_arg;

static void __iomem *ocp_reg_base;
static void __iomem *lcdc_fb_reg_base;
static unsigned int lcd_rev;
static irq_handler_t lcdc_irq_handler;
static wait_queue_head_t frame_done_wq;
static int frame_done_flag;

static void lcdc_write(unsigned int val, unsigned int addr)
{
    __raw_writel(val, (volatile void __iomem *)lcdc_fb_reg_base + (addr));
}

static unsigned int lcdc_read(unsigned int addr)
{
    DEBUG_PRINTF("Reading from %px\n",lcdc_fb_reg_base + (addr));
    return (unsigned int)readl(lcdc_fb_reg_base + (addr));
}

static bool lcdc_is_raster_enabled(void) {
    return ((lcdc_read(LCD_RASTER_CTRL_REG) & LCD_RASTER_ENABLE) != 0);
}

static void lcdc_enable_raster(void)
{
    u32 reg;

    if (lcd_rev == LCD_VERSION_2) {
        lcdc_write(LCD_CLK_MAIN_RESET, LCD_CLK_RESET_REG);
    }

    mdelay(1);

    if (lcd_rev == LCD_VERSION_2) {
        lcdc_write(0, LCD_CLK_RESET_REG);
    }

    mdelay(1);

    reg = lcdc_read(LCD_RASTER_CTRL_REG);
    if (!(reg & LCD_RASTER_ENABLE)) {
        lcdc_write(reg | LCD_RASTER_ENABLE, LCD_RASTER_CTRL_REG);
    }
} 

static void lcdc_disable_raster(enum lcdc_frame_complete wait_for_frame_done)
{
    u32 reg;
    reg = lcdc_read(LCD_RASTER_CTRL_REG);

    if (reg & LCD_RASTER_ENABLE) {
        lcdc_write(reg & ~LCD_RASTER_ENABLE, LCD_RASTER_CTRL_REG);
    }

         if ((wait_for_frame_done == LCDC_FRAME_WAIT) &&
             (lcd_rev == LCD_VERSION_2)) {
         frame_done_flag = 0;
         int ret = wait_event_interruptible_timeout(frame_done_wq,
                 frame_done_flag != 0,
                 msecs_to_jiffies(50));
         if (ret == 0)
             pr_err("LCD Controller timed out\n");
     }
}

static void lcdc_blit(int load_mode, struct lcdc_fb_data *par)
{
    u32 start;
    u32 end;
    u32 reg_ras;
    u32 reg_dma;
    u32 reg_int;

    reg_ras = lcdc_read(LCD_RASTER_CTRL_REG);
    reg_ras &= ~(3 << 20);

    reg_dma = lcdc_read(LCD_DMA_CTRL_REG);

    if (load_mode == LOAD_DATA) {
        start   = par->dma_start;
        end     = par->dma_end;

        reg_ras |= LCD_PALETTE_LOAD_MODE(DATA_ONLY);

        if (lcd_rev == LCD_VERSION_1) {
            reg_dma |= LCD_V1_END_OF_FRAME_INT_ENA;
        } else {
            reg_int = lcdc_read(LCD_INT_ENABLE_SET_REG) |
                            LCD_V2_END_OF_FRAME0_INT_ENA |
                            LCD_V2_END_OF_FRAME1_INT_ENA |
                            LCD_FRAME_DONE | LCD_SYNC_LOST;
            lcdc_write(reg_int, LCD_INT_ENABLE_SET_REG);
        }

        reg_dma |= LCD_DUAL_FRAME_BUFFER_ENABLE;

        lcdc_write(start, LCD_DMA_FRM_BUF_BASE_ADDR_0_REG);
        lcdc_write(end, LCD_DMA_FRM_BUF_BASE_ADDR_0_REG);
        lcdc_write(start, LCD_DMA_FRM_BUF_BASE_ADDR_1_REG);
        lcdc_write(end, LCD_DMA_FRM_BUF_BASE_ADDR_1_REG);
    } else if ( load_mode == LOAD_PALETTE){
        // TODO: add palette mode
    }

    lcdc_write(reg_dma, LCD_DMA_CTRL_REG);
    lcdc_write(reg_ras, LCD_RASTER_CTRL_REG);

    lcdc_enable_raster();
}

static void lcdc_load_palette(struct lcdc_fb_data *par)
{
    // TODO: add palette mode
}

static int lcd_cfg_dma(int burst_size, int fifo_th)
{
    u32 reg;

    reg = lcdc_read(LCD_DMA_CTRL_REG) & 0x00000001;
    switch (burst_size) {
        case 1:
            reg |= LCD_DMA_BURST_SIZE(LCD_DMA_BURST_1);
            break;
        case 2:
            reg |= LCD_DMA_BURST_SIZE(LCD_DMA_BURST_2);
            break;
        case 4:
            reg |= LCD_DMA_BURST_SIZE(LCD_DMA_BURST_4);
            break;
        case 8:
            reg |= LCD_DMA_BURST_SIZE(LCD_DMA_BURST_8);
            break;
        case 16:
        default:
            reg |= LCD_DMA_BURST_SIZE(LCD_DMA_BURST_16);
            break;
    }

    reg |= (fifo_th << 8);

    lcdc_write(reg, LCD_DMA_CTRL_REG);
    return 0;
}

static void lcd_cfg_ac_bias(int period, int transitions_per_int)
{
    u32 reg;

    reg = lcdc_read(LCD_RASTER_TIMING_2_REG) & 0xFFF00000;
    reg |= LCD_AC_BIAS_FREQUENCY(period) |
        LCD_AC_BIAS_TRANSITIONS_PER_INT(transitions_per_int);

    lcdc_write(reg, LCD_RASTER_TIMING_2_REG);
}

static void lcd_cfg_horizontal_sync(int back_porch, int pulse_width, int front_porch)
{
    u32 reg;

    DEBUG_PRINTF("Horizontal back_porch:%u, pulse_width:%u, front_porch:%u\n", 
                  back_porch, pulse_width, front_porch);

    reg = lcdc_read(LCD_RASTER_TIMING_0_REG) & 0xF;
    reg |= (((back_porch-1) & 0xFF) << 24)
        | (((front_porch-1) & 0xFF) << 16)
        | (((pulse_width-1) & 0x3F) << 10);
    lcdc_write(reg, LCD_RASTER_TIMING_0_REG);

    if (lcd_rev == LCD_VERSION_2) {
        reg = lcdc_read(LCD_RASTER_TIMING_2_REG) & ~0x780000FF;
        reg |= (((front_porch-1) & 0x300) >> 8);
        reg |= (((back_porch-1) & 0x300) >> 4);
        reg |= (((pulse_width-1) & 0x3C0) << 21);
        lcdc_write(reg, LCD_RASTER_TIMING_2_REG);
    }
}

static int lcd_cfg_vertical_sync(int back_porch, int pulse_width, int front_porch)
{
    u32 reg;

    DEBUG_PRINTF("Vertical back_porch:%u, pulse_width:%u, front_porch:%u\n", 
                  back_porch, pulse_width, front_porch);
    reg = lcdc_read(LCD_RASTER_TIMING_1_REG) & 0x3FF;
    reg |= ((back_porch & 0xFF) << 24)
        | ((front_porch & 0xFF) << 16)
        | (((pulse_width-1) & 0x3F) << 10);
    lcdc_write(reg, LCD_RASTER_TIMING_1_REG);

    return 0;
}

static int lcd_cfg_display(const struct lcd_ctrl_config *cfg, struct fb_videomode *panel)
{
    u32 reg;
    u32 reg_int;

    reg = lcdc_read(LCD_RASTER_CTRL_REG) & ~(LCD_TFT_MODE 
                                        | LCD_MONO_8BIT_MODE 
                                        | LCD_MONOCHROME_MODE);

    switch (cfg->panel_shade) {
        case MONOCHROME:
            reg |= LCD_MONOCHROME_MODE;
            if (cfg->mono_8bit_mode) {
                reg |= LCD_MONO_8BIT_MODE;
            }
            break;
        case COLOR_ACTIVE:
            reg |= LCD_TFT_MODE;
            if (cfg->tft_alt_mode) {
                reg |= LCD_TFT_ALT_ENABLE;
            }
            break;
        case COLOR_PASSIVE:
            lcd_cfg_ac_bias(cfg->ac_bias, cfg->ac_bias_intrpt);
            if (cfg->bpp == 12 && cfg->stn_565_mode) {
                reg |= LCD_STN_565_ENABLE;
            }
            break;

        default:
            return -EINVAL;
    }

    if (lcd_rev == LCD_VERSION_1) {
        reg |= LCD_V1_UNDERFLOW_INT_ENA;
    } else {
        reg_int = lcdc_read(LCD_INT_ENABLE_SET_REG) |
            LCD_V2_UNDERFLOW_INT_ENA;
        lcdc_write(reg_int, LCD_INT_ENABLE_SET_REG);
    }

    lcdc_write(reg, LCD_RASTER_CTRL_REG);

    reg = lcdc_read(LCD_RASTER_TIMING_2_REG);

    reg |= LCD_SYNC_CTRL;

    if (cfg->sync_edge) {
        reg |= LCD_SYNC_EDGE;
    } else {
        reg &= ~LCD_SYNC_EDGE;
    }

    if ((panel->sync & FB_SYNC_HOR_HIGH_ACT) == 0) {
        reg |= LCD_INVERT_LINE_CLOCK;
    } else {
        reg &= ~LCD_INVERT_LINE_CLOCK;
    }

    if ((panel->sync & FB_SYNC_VERT_HIGH_ACT) == 0) {
        reg |= LCD_INVERT_FRAME_CLOCK;
    } else {
        reg &= ~LCD_INVERT_FRAME_CLOCK;
    }

    lcdc_write(reg, LCD_RASTER_TIMING_2_REG);

    return 0;
}

static int lcd_cfg_frame_buffer(struct lcdc_fb_data *par, u32 width, u32 height,
                                u32 bpp, u32 raster_order)
{
    u32 reg;

    if (bpp > 16 && lcd_rev == LCD_VERSION_1) {
        return -EINVAL;
    }

    DEBUG_PRINTF("width_pre:%lu\n", width);

    if (lcd_rev == LCD_VERSION_1) {
        width &= 0x3F0;
    } else {
        width &= 0x7F0;
    }

    DEBUG_PRINTF("width_post:%lu\n", width);

    reg = lcdc_read(LCD_RASTER_TIMING_0_REG);
    reg &= 0xFFFFFC00;

    if (lcd_rev == LCD_VERSION_1) {
        reg |= ((width >> 4) - 1) << 4;
    } else {
        width = ((width - 1) >> 4);
        reg |= ((width & 0x3F) << 4) | ((width & 0x40) >> 3);
    }

    lcdc_write(reg, LCD_RASTER_TIMING_0_REG);

    reg = lcdc_read(LCD_RASTER_TIMING_1_REG);
    reg = ((height - 1) & 0x3FF) | (reg & 0xFFFFFC00);
    lcdc_write(reg, LCD_RASTER_TIMING_1_REG);

    if (lcd_rev == LCD_VERSION_2) {
        reg = lcdc_read(LCD_RASTER_TIMING_2_REG);
        reg |= ((height - 1) & 0x400) << 16;
        lcdc_write(reg, LCD_RASTER_TIMING_2_REG);
    }

    reg = lcdc_read(LCD_RASTER_CTRL_REG) & ~(1 << 8);
    if (raster_order) {
        reg |= LCD_RASTER_ORDER;
    }

    par->palette_sz = 16*2;

    if (lcd_rev == LCD_VERSION_2) {
        reg &= ~LCD_V2_TFT_24BPP_MODE;
        reg &= ~LCD_V2_TFT_24BPP_UNPACK;
    }

    switch (bpp) {
        case 1:
        case 2:
        case 4:
        case 16:
            break;
        case 24:
            reg |= LCD_V2_TFT_24BPP_MODE;
            break;
        case 32:
            reg |= LCD_V2_TFT_24BPP_MODE;
            reg |= LCD_V2_TFT_24BPP_UNPACK;
            break;
        case 8:
            par->palette_sz = 256*2;
            break;
        default:
            return -EINVAL;
    }

    lcdc_write(reg, LCD_RASTER_CTRL_REG);

    return 0;
}

#define CNVT_TOHW(val, width) ((((val) << (width)) + 0x7FFF - (val)) >> 16)
static int fb_setcolreg(unsigned regno, unsigned red, unsigned green, unsigned blue,
                        unsigned transp, struct fb_info *info)
{
    struct lcdc_fb_data *par = info->par;
    unsigned short *palette = (unsigned short *)par->v_palette_base;
    u_short pal;
    int update_hw = 0;

    if (regno > 255) {
        return 1;
    }

    if (info->fix.visual == FB_VISUAL_DIRECTCOLOR) {
        return 1;
    }

    if (info->var.bits_per_pixel > 16 && lcd_rev == LCD_VERSION_1) {
        return -EINVAL;
    }
    
    switch (info->fix.visual) {
        case FB_VISUAL_TRUECOLOR:
            red = CNVT_TOHW(red, info->var.red.length);
            green = CNVT_TOHW(green, info->var.green.length);
            blue = CNVT_TOHW(blue, info->var.blue.length);
            break;
        case FB_VISUAL_PSEUDOCOLOR:
            switch (info->var.bits_per_pixel) {
                case 4:
                    if (regno > 15) {
                        return -EINVAL;
                    }
                    if (info->var.grayscale) {
                        pal = regno;
                    } else {
                        red >>= 4;
                        green >>= 8;
                        blue >>= 12;

                        pal = red & 0x0f00;
                        pal |= green & 0x00f0;
                        pal |= blue & 0x000f;
                    }
                    if (regno == 0) {
                        pal |= 0x2000;
                    }
                    palette[regno] = pal;
                    break;
                case 8:
                    red >>= 4;
                    green >>= 8;
                    blue >>= 12;

                    pal = red & 0x0f00;
                    pal |= green & 0x00f0;
                    pal |= blue & 0x000f;

                    if (palette[regno] != pal) {
                        update_hw = 1;
                        palette[regno] = pal;
                    }
                break;
            }
        break;
    }

    if (info->fix.visual == FB_VISUAL_TRUECOLOR) {
        u32 v;

        if (regno > 15) {
            return -EINVAL;
        }

        v = (red << info->var.red.offset) |
            (green << info->var.green.offset) |
            (blue << info->var.blue.offset);

        switch (info->var.bits_per_pixel) {
            case 16:
                ((u16 *) (info->pseudo_palette))[regno] = v;
                break;
            case 24:
            case 32:
                ((u32 *) (info->pseudo_palette))[regno] = v;
                break;
        }
        if (palette[0] != 0x4000) {
            update_hw = 1;
            palette[0] = 0x4000;
        }
    }

    if (update_hw) {
        lcdc_load_palette(par);
    }

    return 0;
}
#undef CNVT_TOHW

static void lcdc_fb_lcd_reset(void)
{
    lcdc_write(0, LCD_DMA_CTRL_REG);
    lcdc_write(0, LCD_RASTER_CTRL_REG);

    if (lcd_rev == LCD_VERSION_2) {
        lcdc_write(0, LCD_INT_ENABLE_SET_REG);
        lcdc_write(LCD_CLK_MAIN_RESET, LCD_CLK_RESET_REG);
        lcdc_write(0, LCD_CLK_RESET_REG);
    }
}

static int lcdc_fb_config_clk_divider(struct lcdc_fb_data *par,
                          unsigned lcdc_clk_div,
                          unsigned lcdc_clk_rate)
{
    int ret;

    if (par->lcdc_clk_rate != lcdc_clk_rate) {
        ret = clk_set_rate(par->lcdc_clk, lcdc_clk_rate);
        if (IS_ERR_VALUE(ret)) {
            dev_err(par->dev,
                "unable to set clock rate at %u\n",
                lcdc_clk_rate);
            return ret;
        }
        par->lcdc_clk_rate = clk_get_rate(par->lcdc_clk);
    }

    /* Configure the LCD clock divisor. */
    lcdc_write(LCD_CLK_DIVISOR(lcdc_clk_div) |
            (LCD_RASTER_MODE & 0x1), LCD_CTRL_REG);

    if (lcd_rev == LCD_VERSION_2)
        lcdc_write(LCD_V2_DMA_CLK_EN | LCD_V2_LIDD_CLK_EN |
                LCD_V2_CORE_CLK_EN, LCD_CLK_ENABLE_REG);

    return 0;
}

static unsigned int lcdc_fb_calc_clk_divider(struct lcdc_fb_data *par,
                                             unsigned pixclock,
                                             unsigned *lcdc_clk_rate)
{
    unsigned lcdc_clk_div;

    lcdc_fb_var.pixclock = 15000000; // TODO change this to read from devicetree
    pixclock = lcdc_fb_var.pixclock;

    *lcdc_clk_rate = par->lcdc_clk_rate;

    DEBUG_PRINTF("pixclock0: %lu, clock_rate: %lu\n", pixclock, *lcdc_clk_rate);

    if (pixclock < (*lcdc_clk_rate / CLK_MAX_DIV)) {
        *lcdc_clk_rate = clk_round_rate(par->lcdc_clk,
                                        pixclock * CLK_MAX_DIV);
        lcdc_clk_div = CLK_MAX_DIV;
    } else if (pixclock > (*lcdc_clk_rate / CLK_MIN_DIV)) {
        *lcdc_clk_rate = clk_round_rate(par->lcdc_clk,
                                        pixclock * CLK_MIN_DIV);
        lcdc_clk_div = CLK_MIN_DIV;
    } else {
        lcdc_clk_div = *lcdc_clk_rate / pixclock;
    }

    return lcdc_clk_div;
}

static int lcdc_fb_calc_config_clk_divider(struct lcdc_fb_data *par,
                                          struct fb_videomode *mode)
{
    unsigned long lcdc_clk_rate;
    unsigned long target_pixclock = PICOS2KHZ(mode->pixclock) * 1000;
    unsigned long div;
    int ret;

    // Calculate required divider
    div = DIV_ROUND_UP(clk_get_rate(par->lcdc_clk), target_pixclock);
    div = clamp(div, CLK_MIN_DIV, CLK_MAX_DIV);

    // Adjust clock rate if needed
    if (div == CLK_MIN_DIV || div == CLK_MAX_DIV) {
        lcdc_clk_rate = target_pixclock * div;
        ret = clk_set_rate(par->lcdc_clk, lcdc_clk_rate);
        if (ret) {
            dev_err(par->dev, "Failed to set clock rate: %d\n", ret);
            return ret;
        }
    }

    // Configure the divider
    lcdc_write(LCD_CLK_DIVISOR(div) | LCD_RASTER_MODE, LCD_CTRL_REG);

    if (lcd_rev == LCD_VERSION_2) {
        lcdc_write(LCD_V2_DMA_CLK_EN | LCD_V2_LIDD_CLK_EN |
                  LCD_V2_CORE_CLK_EN, LCD_CLK_ENABLE_REG);
    }

    return 0;
}

static unsigned lcdc_fb_round_clk(struct lcdc_fb_data *par,
                                  unsigned pixclock)
{
    unsigned lcdc_clk_div, lcdc_clk_rate;

    lcdc_clk_div = lcdc_fb_calc_clk_divider(par, lcdc_clk_div, &lcdc_clk_rate);
    return KHZ2PICOS(lcdc_clk_rate / (1000*lcdc_clk_div));
}

static int lcd_init(struct lcdc_fb_data *par,
                    const struct lcd_ctrl_config *cfg, struct fb_videomode *panel)
{
    u32 bpp;
    int ret = 0;
    struct lcdc_encoder *enc = NULL;

    if (par->hdmi_node) {
        const unsigned clkdiv = 2;
        unsigned long pixclock;

        pixclock = PICOS2KHZ(panel->pixclock) * 1000;

        pr_debug("target pixclock %d ps %lu Hz\n", panel->pixclock, pixclock);

        ret = clk_set_rate(par->lcdc_clk, pixclock *clkdiv);
        if (ret < 0) {
            dev_err(par->dev,
                    "failed to set display clock rate to %lu: %d\n",
                    pixclock, ret);
            return ret;
        }

        par->lcdc_clk_rate = clk_get_rate(par->lcdc_clk);
        pixclock = par->lcdc_clk_rate / clkdiv;

        lcdc_write(LCD_CLK_DIVISOR(clkdiv) | 
                  (LCD_RASTER_MODE & 0x1), LCD_CTRL_REG);

        if (lcd_rev == LCD_VERSION_2) {
            lcdc_write(LCD_V2_DMA_CLK_EN | LCD_V2_LIDD_CLK_EN |
                       LCD_V2_CORE_CLK_EN, LCD_CLK_ENABLE_REG);
        }
    } else {
        DEBUG_PRINTF("pixclock: %lu\n", panel->pixclock);

        ret = lcdc_fb_calc_config_clk_divider(par, panel);

        if (IS_ERR_VALUE(ret)) {
            dev_err(par->dev, "unable to configure clock\n");
            return ret;
        }
    }

    //if (panel->sync & FB_SYNC_CLK_INVERT) {
    //    lcdc_write((lcdc_read(LCD_RASTER_TIMING_2_REG) |
    //                LCD_INVERT_PIXEL_CLOCK), LCD_RASTER_TIMING_2_REG);
    //} else {
        lcdc_write((lcdc_read(LCD_RASTER_TIMING_2_REG) &
                    ~LCD_INVERT_PIXEL_CLOCK), LCD_RASTER_TIMING_2_REG);
    //}

    ret = lcd_cfg_dma(cfg->dma_burst_sz, cfg->fifo_th);
    if (ret < 0) {
        return ret;
    }

    lcd_cfg_vertical_sync(panel->upper_margin, panel->vsync_len,
            panel->lower_margin);
    lcd_cfg_horizontal_sync(panel->left_margin, panel->hsync_len,
            panel->right_margin);
    
    ret = lcd_cfg_display(cfg, panel);
    if (ret < 0)
        return ret;
    
    bpp = cfg->bpp;
    
    if (bpp == 12)
        bpp = 16;
    ret = lcd_cfg_frame_buffer(par, (unsigned int)panel->xres,
                (unsigned int)panel->yres, bpp,
                cfg->raster_order);
    if (ret < 0)
        return ret;
    
    lcdc_load_palette(par);
    
    lcdc_write((lcdc_read(LCD_RASTER_CTRL_REG) & 0xfff00fff) |
               (cfg->fdd << 12), LCD_RASTER_CTRL_REG);

    return 0;
}

int register_vsync_cb(vsync_callback_t handler, void *arg, int idx)
{
    if ((vsync_cb_handler == NULL) && (vsync_cb_arg == NULL)) {
        vsync_cb_arg = arg;
        vsync_cb_handler = handler;
    } else {
        return -EEXIST;
    }

    return 0;
}
EXPORT_SYMBOL(register_vsync_cb);

int unregister_vsync_cb(vsync_callback_t handler, void *arg, int idx)
{
    if ((vsync_cb_handler == handler) && (vsync_cb_arg == arg)) {
        vsync_cb_handler = NULL;
        vsync_cb_arg = NULL;
    } else {
        return -ENXIO;
    }

    return 0;
}
EXPORT_SYMBOL(unregister_vsync_cb);

static irqreturn_t lcdc_irq_handler_rev02(int irq, void *arg)
{
    struct lcdc_fb_data *par = arg;
    u32 stat =lcdc_read(LCD_MASKED_STAT_REG);

    if ((stat & LCD_SYNC_LOST) && (stat & LCD_FIFO_UNDERFLOW)) {
        lcdc_disable_raster(LCDC_FRAME_NOWAIT);
        lcdc_write(stat, LCD_MASKED_STAT_REG);
        lcdc_enable_raster();
    } else if (stat & LCD_PL_LOAD_DONE) {
        lcdc_disable_raster(LCDC_FRAME_NOWAIT);
        lcdc_write(stat, LCD_MASKED_STAT_REG);

        lcdc_write(LCD_V2_PL_INT_ENA, LCD_INT_ENABLE_CLR_REG);
        par->palette_loaded_flag = 1;

        wake_up_interruptible(&par->palette_wait);
        lcdc_enable_raster();
    } else {
        lcdc_write(stat, LCD_MASKED_STAT_REG);

        if (stat & LCD_END_OF_FRAME0) {
            par->which_dma_channel_done = 0;
            lcdc_write(par->dma_start,
                       LCD_DMA_FRM_BUF_BASE_ADDR_0_REG);
            lcdc_write(par->dma_end,
                       LCD_DMA_FRM_BUF_CEILING_ADDR_0_REG);
            par->vsync_flag = 1;

            wake_up_interruptible(&par->vsync_wait);
            if (vsync_cb_handler) {
                vsync_cb_handler(vsync_cb_arg);
            }
        }

        if (stat & LCD_END_OF_FRAME1) {
            par->which_dma_channel_done = 1;
            lcdc_write(par->dma_start,
                       LCD_DMA_FRM_BUF_BASE_ADDR_1_REG);
            lcdc_write(par->dma_end,
                       LCD_DMA_FRM_BUF_CEILING_ADDR_1_REG);
            par->vsync_flag = 1;

            wake_up_interruptible(&par->vsync_wait);
            if (vsync_cb_handler) {
                vsync_cb_handler(vsync_cb_arg);
            }
        }

        if (stat & BIT(0)) {
            frame_done_flag = 1;
            wake_up_interruptible(&frame_done_wq);
        }
    }

    lcdc_write(0, LCD_END_OF_INT_IND_REG);
    return IRQ_HANDLED;
}

static irqreturn_t lcdc_irq_handler_rev01(int irq, void *arg)
{
    struct lcdc_fb_data *par = arg;
    u32 stat = lcdc_read(LCD_STAT_REG);
    u32 reg_ras;

    if ((stat & LCD_SYNC_LOST) && (stat & LCD_FIFO_UNDERFLOW)) {
        lcdc_disable_raster(LCDC_FRAME_NOWAIT);

        lcdc_write(stat, LCD_STAT_REG);

        reg_ras  = lcdc_read(LCD_RASTER_CTRL_REG);
        reg_ras &= ~LCD_V1_PL_INT_ENA;
        lcdc_write(reg_ras, LCD_RASTER_CTRL_REG);

        par->palette_loaded_flag = 1;
        wake_up_interruptible(&par->palette_wait);
    } else {
        lcdc_write(stat, LCD_STAT_REG);
        if (stat & LCD_END_OF_FRAME0) {
            par->which_dma_channel_done = 0;
            lcdc_write(par->dma_start,
                   LCD_DMA_FRM_BUF_BASE_ADDR_0_REG);
            lcdc_write(par->dma_end,
                   LCD_DMA_FRM_BUF_CEILING_ADDR_0_REG);
            par->vsync_flag = 1;
            wake_up_interruptible(&par->vsync_wait);
        }
        
        if (stat & LCD_END_OF_FRAME1) {
            par->which_dma_channel_done = 1;
            lcdc_write(par->dma_start,
                   LCD_DMA_FRM_BUF_BASE_ADDR_1_REG);
            lcdc_write(par->dma_end,
                   LCD_DMA_FRM_BUF_CEILING_ADDR_1_REG);
            par->vsync_flag = 1;
            wake_up_interruptible(&par->vsync_wait);
        }
    }

    return IRQ_HANDLED;
}

static int fb_check_var(struct fb_var_screeninfo *var,
                        struct fb_info *info)
{
    int err = 0;
    struct lcdc_fb_data *par = info->par;
    int bpp = var->bits_per_pixel >> 3;
    unsigned long line_size = var->xres_virtual * bpp;

    if (var->bits_per_pixel > 16 && lcd_rev == LCD_VERSION_1) {
        return -EINVAL;
    }

    switch (var->bits_per_pixel) {
        case 1:
        case 8:
            var->red.offset = 0;
            var->red.length = 8;
            var->green.offset = 0;
            var->green.offset = 8;
            var->blue.length = 0;
            var->blue.length = 8;
            var->transp.offset = 0;
            var->transp.length = 0;
            var->nonstd = 0;
            break;
        case 4:
            var->red.offset = 0;
            var->red.length = 4;
            var->green.offset = 0;
            var->green.offset = 4;
            var->blue.length = 0;
            var->blue.length = 4;
            var->transp.offset = 0;
            var->transp.length = 0;
            var->nonstd = FB_NONSTD_REV_PIX_IN_B;
            break;
        case 16:
            var->red.offset = 11;
            var->red.length = 5;
            var->green.offset = 5;
            var->green.length = 6;
            var->blue.offset = 0;
            var->blue.length = 5;
            var->transp.offset = 0;
            var->transp.length = 0;
            var->nonstd = 0;
            break;
        case 24:
            var->red.offset = 0;
            var->red.length = 8;
            var->green.offset = 16;
            var->green.length = 8;
            var->blue.offset = 8;
            var->blue.length = 8;
            var->nonstd = 0;
            break;
        case 32:
            var->red.offset = 16;
            var->red.length = 8;
            var->green.offset = 8;
            var->green.length = 8;
            var->blue.offset = 0;
            var->blue.length = 8;
            var->transp.offset = 24;
            var->transp.length = 0;
            var->nonstd = 0;
            break;
        default:
            err = -EINVAL;
    }

    var->red.msb_right = 0;
    var->green.msb_right = 0;
    var->blue.msb_right = 0;
    var->transp.msb_right = 0;

    if (line_size * var->yres_virtual > par->vram_size) {
        var->yres_virtual = par->vram_size / line_size;
    }

    if (var->yres > var->yres_virtual) {
        var->yres = var->yres_virtual;
    }

    if (var->xres > var->xres_virtual) {
        var->xres = var->xres_virtual;
    }

    if (var->xres + var->xoffset > var->xres_virtual) {
        var->xoffset = var->xres_virtual - var->xres;
    }

    if (var->yres + var->yoffset > var->yres_virtual) {
        var->yoffset = var->yres_virtual - var->yres;
    }

    if (par->hdmi_node == NULL) {
        var->pixclock = lcdc_fb_round_clk(par, var->pixclock);
    }

    return err;
}

static int fb_remove(struct platform_device *dev)
{
    struct fb_info *info = dev_get_drvdata(&dev->dev);

    if (info) {
        struct lcdc_fb_data *par = info->par;

        if (par->panel_power_ctrl) {
            par->panel_power_ctrl(0);
        }

        lcdc_disable_raster(LCDC_FRAME_WAIT);
        lcdc_write(0, LCD_RASTER_CTRL_REG);

        unregister_framebuffer(info);
        fb_dealloc_cmap(&info->cmap);

        dma_free_coherent(NULL, PALETTE_SIZE, 
                          par->v_palette_base, par->p_palette_base);
        dma_free_coherent(NULL, par->vram_size, 
                          par->vram_virt, par->vram_phys);
        
        pm_runtime_put_sync(&dev->dev);
        pm_runtime_disable(&dev->dev);
        framebuffer_release(info);
    }

    return 0;
}

static int fb_wait_for_vsync(struct fb_info *info)
{
    struct lcdc_fb_data *par = info->par;
    int ret;

    par->vsync_flag = 0;
    ret = wait_event_interruptible_timeout(par->vsync_wait,
                                           par->vsync_flag != 0,
                                           par->vsync_timeout);

    if (ret < 0) {
        return ret;
    } else if (ret == 0) {
        return -ETIMEDOUT;
    }

    return 0;
}

static int fb_ioctl(struct fb_info *info, unsigned int cmd,
                    unsigned long arg)
{
    struct lcd_sync_arg sync_arg;

    switch (cmd) {
        case FBIOGET_CONTRAST:
        case FBIOPUT_CONTRAST:
        case FBIGET_BRIGHTNESS:
        case FBIPUT_BRIGHTNESS:
        case FBIGET_COLOR:
        case FBIPUT_COLOR:
            return -ENOTTY;
        case FBIPUT_HSYNC:
            if (copy_from_user(&sync_arg, (void __user *)arg,
                               sizeof(struct lcd_sync_arg))) {
                return -EFAULT;
            }

            lcd_cfg_horizontal_sync(sync_arg.back_porch,
                                    sync_arg.pulse_width,
                                    sync_arg.front_porch);
            break;
        case FBIPUT_VSYNC:
            if (copy_from_user(&sync_arg, (void __user *)arg,
                               sizeof(struct lcd_sync_arg))) {
                return -EFAULT;
            }

            lcd_cfg_vertical_sync(sync_arg.back_porch,
                                    sync_arg.pulse_width,
                                    sync_arg.front_porch);
            break;
        case FBIO_WAITFORVSYNC:
            return fb_wait_for_vsync(info);
        default:
            return -EINVAL;
    }

    return 0;
}

static int cfb_blank(int blank, struct fb_info *info)
{
    struct lcdc_fb_data *par = info->par;
    int ret = 0;

    if (par->blank == blank)
    {
        return 0;
    }

    par->blank = blank;

    switch (blank) {
        case FB_BLANK_UNBLANK:
            lcdc_enable_raster();

            if (par->panel_power_ctrl) {
                par->panel_power_ctrl(1);
            }
            break;
        case FB_BLANK_NORMAL:
        case FB_BLANK_VSYNC_SUSPEND:
        case FB_BLANK_HSYNC_SUSPEND:
        case FB_BLANK_POWERDOWN:
            if (par->panel_power_ctrl) {
                par->panel_power_ctrl(0);
            }

            lcdc_disable_raster(LCDC_FRAME_WAIT);
            break;
        default:
            ret = -EINVAL;
    }

    return ret;
}

static int lcdc_pan_display(struct fb_var_screeninfo *var,
                            struct fb_info *fbi)
{
    int ret = 0;
    struct fb_var_screeninfo new_var;
    struct lcdc_fb_data      *par = fbi->par;
    struct fb_fix_screeninfo *fix = &fbi->fix;

    unsigned int start, end;
    unsigned long irq_flags;

    if (var->xoffset != fbi->var.xoffset ||
        var->yoffset != fbi->var.yoffset) {

        memcpy(&new_var, &fbi->var, sizeof(new_var));
        new_var.xoffset = var->xoffset;
        new_var.yoffset = var->yoffset;

        if (fb_check_var(&new_var, fbi)) {
            ret = -EINVAL;
        } else {
            memcpy(&fbi->var, &new_var, sizeof(new_var));

            start = fix->smem_start + 
                new_var.yoffset * fix->line_length + 
                new_var.xoffset * fbi->var.bits_per_pixel/8;
            end = start + fbi->var.yres * fix->line_length - 1;
            par->dma_start  = start;
            par->dma_end    = end;

            spin_lock_irqsave(&par->lock_for_chan_update,
                             irq_flags);

            if (par->which_dma_channel_done == 0) {
                lcdc_write(par->dma_start,
                           LCD_DMA_FRM_BUF_BASE_ADDR_0_REG);
                lcdc_write(par->dma_end,
                           LCD_DMA_FRM_BUF_CEILING_ADDR_0_REG);
            } else if (par->which_dma_channel_done == 1) {
                lcdc_write(par->dma_start,
                           LCD_DMA_FRM_BUF_BASE_ADDR_1_REG);
                lcdc_write(par->dma_end,
                           LCD_DMA_FRM_BUF_CEILING_ADDR_1_REG);
            }

            spin_unlock_irqrestore(&par->lock_for_chan_update,
                                   irq_flags);
        }
    }

    return ret;
}

static int lcdc_set_par(struct fb_info *info)
{
    struct lcdc_fb_data *par = info->par;
    int ret;
    struct device_node *np = par->dev->of_node;
    bool raster = lcdc_is_raster_enabled();
    struct device_node *timings_np, *native_np;
    u32 clock_freq;

    if (raster) {
        lcdc_disable_raster(LCDC_FRAME_WAIT);
    }

    fb_var_to_videomode(&par->mode, &info->var);

    lcdc_fb_var.pixclock = 15000000; // TODO change this to read from devicetree

    par->cfg.bpp = info->var.bits_per_pixel;

    info->fix.visual = (par->cfg.bpp <= 8) ?
                        FB_VISUAL_PSEUDOCOLOR : FB_VISUAL_TRUECOLOR;
    info->fix.line_length = (par->mode.xres * par->cfg.bpp) / 8;

    ret = lcd_init(par, &par->cfg, &par->mode);

    if (ret < 0) {
        dev_err(par->dev, "lcd init failed\n");
        return ret;
    }

    par->dma_start = info->fix.smem_start +
                     info->var.yoffset * info->fix.line_length +
                     info->var.xoffset * info->var.bits_per_pixel / 8;
    par->dma_end   = par->dma_start + 
                     info->var.yres * info->fix.line_length - 1;

    lcdc_write(par->dma_start, LCD_DMA_FRM_BUF_BASE_ADDR_0_REG);
    lcdc_write(par->dma_end, LCD_DMA_FRM_BUF_CEILING_ADDR_0_REG);
    lcdc_write(par->dma_start, LCD_DMA_FRM_BUF_BASE_ADDR_1_REG);
    lcdc_write(par->dma_end, LCD_DMA_FRM_BUF_CEILING_ADDR_1_REG);

    if (raster) {
        lcdc_enable_raster();
    }

    return 0;
}

static struct fb_ops lcdc_fb_ops = {
    .owner          = THIS_MODULE,
    .fb_check_var   = fb_check_var,
    .fb_set_par     = lcdc_set_par,
    .fb_setcolreg   = fb_setcolreg,
    .fb_pan_display = lcdc_pan_display,
    .fb_ioctl       = fb_ioctl,
    .fb_fillrect    = cfb_fillrect,
    .fb_copyarea    = cfb_copyarea,
    .fb_imageblit   = cfb_imageblit,
    .fb_blank       = cfb_blank,
};

static struct lcd_ctrl_config *lcdc_fb_create_cfg(struct platform_device *dev)
{
    struct lcd_ctrl_config *cfg;

    cfg = devm_kzalloc(&dev->dev, sizeof(struct fb_videomode), GFP_KERNEL);

    if (!cfg) {
        return NULL;
    }

    if (lcd_rev == LCD_VERSION_1) {
        cfg->bpp = 16;
    } else {
        cfg->bpp = 24;
    }

    cfg->panel_shade = COLOR_ACTIVE;
    return cfg;
}

static struct fb_videomode *lcdc_fb_get_videomode(struct platform_device *dev)
{
    struct lcdc_platform_data *fb_pdata = dev->dev.platform_data;
    struct fb_videomode *lcdc_info;
    struct device_node *np = dev->dev.of_node;
    int i;

    if (np) {
        lcdc_info = devm_kzalloc(&dev->dev,
                                 sizeof(struct fb_videomode),
                                 GFP_KERNEL);

        if (!lcdc_info) {
            return NULL;
        }

        if (of_get_fb_videomode(np, lcdc_info, OF_USE_NATIVE_MODE)) {
            return NULL;
        }

        return lcdc_info;
    }

    for (i=0, lcdc_info = lcd_panels; i<ARRAY_SIZE(lcd_panels); i++, lcdc_info++ ) {
        if (strcmp(fb_pdata->type, lcdc_info->name) == 0) {
            break;
        }
    }

    if (i == ARRAY_SIZE(lcd_panels)) {
        dev_err(&dev->dev, "no panel found\n");
        return NULL;
    }

    dev_info(&dev->dev, "found %s panel\n", lcdc_info->name);

    return lcdc_info;
}

static int fb_probe(struct platform_device *device)
{
    struct lcdc_platform_data *fb_pdata = device->dev.platform_data;
    static struct resource *lcdc_regs;
    struct lcd_ctrl_config *lcd_cfg;
    struct fb_videomode *lcdc_info;
    struct fb_info *lcdc_fb_info;
    struct lcdc_fb_data *par;
    struct clk *tmp_lcdc_clk;
    int ret;
    unsigned long ulcm;
    struct device_node *hdmi_node = NULL;

    struct pinctrl *pinctrl;
    struct pinctrl_state *default_state;

    lcdc_fb_var.pixclock = 15000000; // TODO change this to read from devicetree

    DEBUG_PRINTF("In fb probe\n");
    
    // Get the pinctrl
    pinctrl = devm_pinctrl_get(&device->dev);
    if (IS_ERR(pinctrl)) {
        dev_err(&device->dev, "Failed to get pinctrl\n");
        return PTR_ERR(pinctrl);
    }
    
    // Get the default state
    default_state = pinctrl_lookup_state(pinctrl, "default");
    if (IS_ERR(default_state)) {
        dev_err(&device->dev, "Failed to get default pinctrl state\n");
        devm_pinctrl_put(pinctrl);
        return PTR_ERR(default_state);
    }
    
    // Apply the default state
    ret = pinctrl_select_state(pinctrl, default_state);
    if (ret < 0) {
        dev_err(&device->dev, "Failed to set default pinctrl state\n");
        devm_pinctrl_put(pinctrl);
        return ret;
    }

    if (fb_pdata == NULL && !device->dev.of_node) {
        dev_err(&device->dev, "Can not get platform data\n");
        return -ENOENT;
    }

    lcdc_info = lcdc_fb_get_videomode(device);

    if (lcdc_info == NULL) {
        return -ENODEV;
    }

    DEBUG_PRINTF("lcdc_info @ %p (xres=%d, yres=%d)\n",
         lcdc_info, lcdc_info ? lcdc_info->xres : -1,
         lcdc_info ? lcdc_info->yres : -1);

    // Replace the current mapping code with:
    lcdc_regs = platform_get_resource(device, IORESOURCE_MEM, 0);
    if (!lcdc_regs) {
        dev_err(&device->dev, "failed to get MEM resource\n");
        return -ENXIO;
    }

    DEBUG_PRINTF("lcdc_regs: start:%x, size:%x\n", lcdc_regs->start, resource_size(lcdc_regs));

    lcdc_fb_reg_base = devm_ioremap_resource(&device->dev, lcdc_regs);
    if (IS_ERR(lcdc_fb_reg_base)) {
        dev_err(&device->dev, "failed to ioremap registers\n");
        return PTR_ERR(lcdc_fb_reg_base);
    }

    ocp_reg_base = ioremap(0x4C000000, 0x1000);
    unsigned int data_ocp = readl(ocp_reg_base + 0x54);
    writel(data_ocp & 0xFF000000 | 0xFFFF00, ocp_reg_base + 0x54);

    DEBUG_PRINTF("lcdc_fb_reg_base @%px (phys 0x%08lx)\n",
            lcdc_fb_reg_base, (unsigned long)lcdc_regs->start);

    tmp_lcdc_clk = clk_get(&device->dev, "fck");
    if (IS_ERR(tmp_lcdc_clk)) {
        dev_err(&device->dev, "Can not get device clock\n");
        return PTR_ERR(tmp_lcdc_clk);
    }

    struct device_node *timings_np;
    timings_np = of_parse_phandle(device->dev.of_node, "display-timings", 0);

    pm_runtime_enable(&device->dev);
    pm_runtime_get_sync(&device->dev);

    DEBUG_PRINTF("Bef read\n");
    unsigned long data = lcdc_read(LCD_PID_REG);
    DEBUG_PRINTF("Aft read\n");
    switch (data) {
        case 0x4C100102:
            lcd_rev = LCD_VERSION_1;
            break;
        case 0x4F200800:
        case 0x4F201000:
            lcd_rev = LCD_VERSION_2;
            break;
        default:
            dev_warn(&device->dev, "Unknown PID Reg value 0x%x, defaulting to rev 1\n",
                    lcdc_read(LCD_PID_REG));
            lcd_rev = LCD_VERSION_1;
            break;
    }
    DEBUG_PRINTF("lcd_rev:%x", lcd_rev);

    if (device->dev.of_node) {
        lcd_cfg = lcdc_fb_create_cfg(device);
    } else {
        lcd_cfg = fb_pdata->controller_data;
    }

    if (!lcd_cfg) {
        return -EINVAL;
        goto err_pm_runtime_disable;
    }

    pr_info("Bef framebuffer_alloc\n");

    lcdc_fb_info = framebuffer_alloc(sizeof(struct lcdc_fb_data),
            &device->dev);
    if (!lcdc_fb_info) {
        dev_dbg(&device->dev, "Memory allocation falied for lcdc_fb_info\n");
        ret = -ENOMEM;
        goto err_pm_runtime_disable;
    }

    par = lcdc_fb_info->par;
    par->dev = &device->dev;
    par->lcdc_clk = tmp_lcdc_clk;
    par->lcdc_clk_rate = clk_get_rate(par->lcdc_clk);

    if (fb_pdata && fb_pdata->panel_power_ctrl) {
        par->panel_power_ctrl = fb_pdata->panel_power_ctrl;
        par->panel_power_ctrl(1);
    }

    if (device->dev.of_node) {
        par->hdmi_node = hdmi_node;
    }

    pr_info("bef fb_videomode_to_var\n");

    fb_videomode_to_var(&lcdc_fb_var, lcdc_info);
    par->cfg = *lcd_cfg;

    lcdc_fb_lcd_reset();

    par->vram_size = lcdc_info->xres * lcdc_info->yres * lcd_cfg->bpp;
    ulcm = lcm((lcdc_info->xres * lcd_cfg->bpp)/8, PAGE_SIZE);
    par->vram_size = roundup(par->vram_size/8, ulcm);
    par->vram_size = par->vram_size * LCD_NUM_BUFFERS;

    par->vram_virt = dma_alloc_coherent(par->dev, par->vram_size,
            (resource_size_t *) &par->vram_phys,
            GFP_KERNEL | GFP_DMA);

    if (!par->vram_virt) {
        dev_err(&device->dev,
                "GLCD: kmalloc for frame buffer failed\n");
        ret = -EINVAL;
        goto err_release_fb;
    }

    lcdc_fb_info->screen_base   = (char __iomem *) par->vram_virt;
    lcdc_fb_fix.smem_start      = par->vram_phys;  
    lcdc_fb_fix.smem_len        = par->vram_size;  
    lcdc_fb_fix.line_length     = (lcdc_info->xres * lcd_cfg->bpp) / 8;

    par->dma_start              = par->vram_phys;
    par->dma_end                = par->dma_start + 
        lcdc_info->yres * lcdc_fb_fix.line_length -1;

    DEBUG_PRINTF("Start: %px, end: %px\n", par->dma_start, par->dma_end);

    par->v_palette_base         = dma_alloc_coherent(par->dev, PALETTE_SIZE,
            (resource_size_t *)&par->p_palette_base,
            GFP_KERNEL | GFP_DMA);

    if (!par->v_palette_base) {
        dev_err(&device->dev,
                "GLCD: kmalloc for palette buffer failed\n");
        ret = -EINVAL;
        goto err_release_fb_mem;
    }

    memset(par->v_palette_base, 0, PALETTE_SIZE);

    par->irq = platform_get_irq(device, 0);
    if (par->irq < 0) {
        ret = -ENOENT;
        goto err_release_pl_mem;
    }

    if (lcd_rev == LCD_VERSION_1) {
        lcdc_irq_handler = lcdc_irq_handler_rev01;
    } else {
        init_waitqueue_head(&frame_done_wq);
        lcdc_irq_handler = lcdc_irq_handler_rev02;
    }

    ret = devm_request_irq(&device->dev, par->irq, lcdc_irq_handler,
            0, DRIVER_NAME, par);

    if (ret) {
        goto err_release_pl_mem;
    }

    lcdc_fb_var.grayscale = lcd_cfg->panel_shade == MONOCHROME ? 1 : 0;
    lcdc_fb_var.bits_per_pixel = lcd_cfg->bpp;
    lcdc_fb_info->flags = FBINFO_FLAG_DEFAULT;
    lcdc_fb_info->fix = lcdc_fb_fix;
    lcdc_fb_info->var = lcdc_fb_var;
    lcdc_fb_info->fbops = &lcdc_fb_ops;
    lcdc_fb_info->pseudo_palette = par->pseudo_palette;
    lcdc_fb_info->fix.visual = (lcdc_fb_info->var.bits_per_pixel <= 8) ?
        FB_VISUAL_PSEUDOCOLOR : FB_VISUAL_TRUECOLOR;

    ret = fb_alloc_cmap(&lcdc_fb_info->cmap, PALETTE_SIZE, 0);

    if (ret) {
        goto err_release_pl_mem;
    }

    lcdc_fb_info->cmap.len = par->palette_sz;

    dev_set_drvdata(&device->dev, lcdc_fb_info);

    init_waitqueue_head(&par->vsync_wait);
    par->vsync_timeout = HZ/5;
    par->which_dma_channel_done = -1;
    spin_lock_init(&par->lock_for_chan_update);

    init_waitqueue_head(&par->palette_wait);

    lcdc_fb_var.activate = FB_ACTIVATE_FORCE;
    fb_set_var(lcdc_fb_info, &lcdc_fb_var);

    if (register_framebuffer(lcdc_fb_info) < 0) {
        dev_err(&device->dev,
                "GLCD: Frame Buffer Registration Failed!\n");
        ret = -EINVAL;
        goto err_dealloc_cmap;
    }

    DEBUG_PRINTF("vram_size:%lu, dma_start:%px, dma_end:%px\n"
                  "bpp:%lu\n, clock_rate:%lu", par->vram_size, par->dma_start, par->dma_end,
                  par->cfg.bpp, par->lcdc_clk_rate);
    lcdc_enable_raster();

    return 0;

err_dealloc_cmap:
    fb_dealloc_cmap(&lcdc_fb_info->cmap);

err_release_pl_mem:
    dma_free_coherent(NULL, PALETTE_SIZE, par->v_palette_base,
            par->p_palette_base);

err_release_fb_mem:
    dma_free_coherent(NULL, par->vram_size, par->vram_virt,
            par->vram_phys);

err_release_fb:
    framebuffer_release(lcdc_fb_info);

err_pm_runtime_disable:
    pm_runtime_put_sync(&device->dev);
    pm_runtime_disable(&device->dev);

    return ret;
}


static int fb_resume(struct platform_device *pdev)
{
    struct fb_info *info = dev_get_drvdata(&pdev->dev);
    struct lcdc_fb_data *par = info->par;
    
    clk_prepare_enable(par->lcdc_clk);
    if (par->panel_power_ctrl)
        par->panel_power_ctrl(1);
    
    lcdc_enable_raster();
    return 0;
}

static int fb_suspend(struct platform_device *pdev, pm_message_t mes)
{
    struct fb_info *info = dev_get_drvdata(&pdev->dev);
    struct lcdc_fb_data *par = info->par;

    lcdc_disable_raster(LCDC_FRAME_WAIT);
    if (par->panel_power_ctrl)
        par->panel_power_ctrl(0);

    clk_disable_unprepare(par->lcdc_clk);
    return 0;
}


static const struct of_device_id lcdc_fb_of_match[] = {
    {.compatible = "lcdc_drv", },
    {},
};

MODULE_DEVICE_TABLE(of, lcdc_fb_of_match);

static struct platform_driver lcdc_fb_driver = {
    .probe = fb_probe,
    .remove = fb_remove,
    .suspend = fb_suspend,
    .resume = fb_resume,
    .driver = {
        .name = DRIVER_NAME,
        .owner = THIS_MODULE,
        .of_match_table = of_match_ptr(lcdc_fb_of_match),
    },
};

static int __init lcdc_fb_init(void)
{
    return platform_driver_register(&lcdc_fb_driver);
}

static void __exit lcdc_fb_cleanup(void)
{
    platform_driver_unregister(&lcdc_fb_driver);
}

module_init(lcdc_fb_init);
module_exit(lcdc_fb_cleanup);

MODULE_DESCRIPTION("Framebuffer driver for evm2000");
MODULE_AUTHOR("J00R");
MODULE_LICENSE("GPL");
