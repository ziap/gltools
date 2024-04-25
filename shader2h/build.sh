#!/bin/sh -xe

CC="${CC:-clang}"

CFLAGS="-std=c99 -Wall -Wextra -pedantic"
LDLIBS="-lEGL -lGLESv2"

BUILD_FLAGS="-O3 -march=native -mtune=native -s"
DEBUG_FLAGS="-Og -ggdb"

$CC $CFLAGS $LDLIBS $BUILD_FLAGS -o shader2h shader2h.c
