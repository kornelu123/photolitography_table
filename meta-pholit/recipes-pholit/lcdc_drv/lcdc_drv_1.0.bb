SUMMARY = "Photolithography evm2000 driver"
DESCRIPTION = "TI EVM2000 kernel driver for photolithography table"
LICENSE = "CLOSED"

inherit module

SRC_URI = " \
    file://Makefile \
    file://lcdc_drv.c \
    file://lcdc_drv.h \
"

# Add required dependencies
DEPENDS += "virtual/kernel"

# Module configuration
MODULE_NAME = "lcdc_drv"

RPROVIDES:${PN} = "lcdc-drv"

# Prevent debug symbols from being included
INHIBIT_PACKAGE_DEBUG_SPLIT = "1"
INHIBIT_PACKAGE_STRIP = "1"

# Use a simpler approach without kernel-module-split issues
EXTRA_OEMAKE += "KERNEL_DIR=${STAGING_KERNEL_DIR}"

S = "${WORKDIR}"

do_compile() {
    oe_runmake -C "${STAGING_KERNEL_DIR}" M="${S}" modules
}

do_install() {
    install -d ${D}${base_libdir}/modules/${KERNEL_VERSION}/kernel/drivers/misc
    install -m 0644 ${S}/lcdc_drv.ko ${D}${base_libdir}/modules/${KERNEL_VERSION}/kernel/drivers/misc/
    
    install -d ${D}${includedir}
    install -m 0644 ${S}/lcdc_drv.h ${D}${includedir}/
}

# Package files - be explicit about what goes where
FILES:${PN} += " \
    ${base_libdir}/modules/${KERNEL_VERSION}/kernel/drivers/misc/lcdc_drv.ko \
"

FILES:${PN}-dev += " \
    ${includedir}/lcdc_drv.h \
"

# Add automatic module loading
KERNEL_MODULE_AUTOLOAD += "lcdc_drv"

# Explicitly define packages to avoid auto-splitting issues
PACKAGES = "${PN} ${PN}-dev"

# Prevent the kernel-module-split from creating empty package directories
MODULES_INSTALL_NO_DEPS = "1"
