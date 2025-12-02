DESCRIPTION = "photolitography application" 
SECTION = "examples" 
LICENSE = "CLOSED" 
PR = "r0" 

DEPENDS = "cmake libgpiod i2c-tools opencv "

SRC_URI="file://CMakeLists.txt \
	 file://hal/ti_gpio.c \
	 file://hal/ti_i2c.c \
	 file://hal/ti_gpio.h \
	 file://hal/ti_i2c.h \
	 file://main.cpp \
	 file://image.cpp \
	 file://image.hpp \
	 file://log.h \
	 file://table.c"

S = "${WORKDIR}"

inherit pkgconfig cmake

do_install() {
    install -d ${D}${bindir}
    install -m 0755 pgraphy ${D}${bindir}
}
