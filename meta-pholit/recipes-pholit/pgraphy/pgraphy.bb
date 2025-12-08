DESCRIPTION = "photolitography application" 
SECTION = "examples" 
LICENSE = "CLOSED" 
PR = "r0" 

DEPENDS = "cmake libgpiod i2c-tools opencv lcdc "

SRC_URI="file://CMakeLists.txt \
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
