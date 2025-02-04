SUMMARY = "A console-only image with full-featured Linux system functionality"

# Enable package management and debug tools
IMAGE_FEATURES += "package-management tools-debug"

# Core and custom packages
IMAGE_INSTALL = "\
    packagegroup-core-boot \
    packagegroup-core-full-cmdline \
    packagegroup-basic \
    lcdc-drv \
    pgraphy \
    libgpiod libgpiod-tools libgpiod-dev \
    i2c-tools opencv tmux vim gdb vtcon_dis \
    libusb-compat kernel-modules udev usbutils minicom \
"

IMAGE_INSTALL:remove = "ti-sgx-ddk-km ti-sgx-ddk-um"

# SDK extensions
TOOLCHAIN_TARGET_TASK += "lcdc-drv opencv-staticdev"

# Custom dev files
FILES:${PN}-dev += "${includedir}/lcdc_drv.h"

# Machine compatibility
COMPATIBLE_MACHINE = "am335x-evm"

KERNEL_MODULE_AUTOLOAD="cdc-acm lcdc-drvd"

inherit core-image
