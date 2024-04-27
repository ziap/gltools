#!/bin/sh -xe

CC="${CC:-clang}"

DEPS="egl glesv2"

CFLAGS="-std=c99 -Wall -Wextra -pedantic `pkgconf --cflags $DEPS`"
LDLIBS="`pkgconf --libs $DEPS`"

BUILD_FLAGS="-O3 -march=native -mtune=native -s"
DEBUG_FLAGS="-Og -ggdb"

$CC $CFLAGS $LDLIBS $DEBUG_FLAGS -o shader2h shader2h.c
