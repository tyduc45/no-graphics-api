#!/bin/bash

# usage:
#   ./compile.sh test.c
#   ./compile.sh test.c debug
#   ./compile.sh test.c --debug

if [ $# -lt 1 ]; then
    echo "Usage: $0 <source.c> [debug|--debug]"
    exit 1
fi

SRC_FILE=$1
MODE=$2

if [ ! -f "$SRC_FILE" ]; then
    echo "Error: file not found: $SRC_FILE"
    exit 1
fi

OUT_DIR="build"
mkdir -p "$OUT_DIR"

BASE_NAME=$(basename "$SRC_FILE" .c)
OUT_FILE="$OUT_DIR/$BASE_NAME"

CFLAGS="$(pkg-config --cflags libdrm_amdgpu)"
LIBS="$(pkg-config --libs libdrm_amdgpu)"

if [ "$MODE" = "debug" ] || [ "$MODE" = "--debug" ] || [ "$MODE" = "-g" ]; then
    echo "Build mode: debug"

    gcc "$SRC_FILE" \
        -o "$OUT_FILE" \
        -g -O0 -Wall -Wextra \
        $CFLAGS $LIBS

    if [ $? -ne 0 ]; then
        echo "Compile failed."
        exit 1
    fi

    echo "Compile success: $OUT_FILE"
    echo "Starting gdb..."
    gdb "$OUT_FILE"
else
    echo "Build mode: normal"

    gcc "$SRC_FILE" \
        -o "$OUT_FILE" \
        $CFLAGS $LIBS

    if [ $? -ne 0 ]; then
        echo "Compile failed."
        exit 1
    fi

    echo "Compile success: $OUT_FILE"
    "./$OUT_FILE"
fi