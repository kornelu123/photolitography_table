#pragma once

#include <stdbool.h>

extern bool debug;

extern "C" {
#define dbg_printf(args...) \
if (debug) { \
    printf("[DEBUG]"); \
    printf(args); \
}
};
