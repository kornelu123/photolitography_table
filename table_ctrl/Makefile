CONFIG_CONST_UART = 0

CONFIG_CMD_INT := $(CONFIG_CONST_UART)

ifeq ($(CONFIG_CMD),uart)
	CONFIG_CMD_INT=$(CONFIG_CONST_UART)
else
	CONFIG_CMD_INT=$(CONFIG_CONST_UART)
endif

CFLAGS += -DCONFIG_CMD=$(CONFIG_CMD_INT)

CFLAGS += -DCONFIG_CONST_UART=$(CONFIG_CONST_UART)

ifneq ($(wildcard .config),)
include .config
else
$(error Create .config file)
endif

all: build/ ninja

build/: 
	CFLAGS="$(CFLAGS)" ./build.sh

ninja: build/
	ninja -C build/

clean:
	rm -rf build/

flash:
	./flash.sh
