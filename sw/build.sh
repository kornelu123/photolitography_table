#!/bin/sh

cmake -S . -B build -GNinja \
-DPICO_SDK_PATH=${PICO_SDK_PATH}
