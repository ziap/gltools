#!/bin/sh -xe

CC="${CC:-clang}"

DEPS="egl glesv2"

CFLAGS="-std=c99 -Wall -Wextra -pedantic `pkg-config --cflags $DEPS`"
LDLIBS="`pkg-config --libs $DEPS`"

BUILD_FLAGS="-O3 -march=native -mtune=native -s"
DEBUG_FLAGS="-Og -ggdb"

$CC $CFLAGS $LDLIBS $BUILD_FLAGS -o shader2h shader2h.c
