#!/bin/bash

# usage 
# ./compile.sh test.c

if [ $# -lt 1 ]; then
    echo "Usage: $0 <source.c>"
    exit 1
fi

SRC_FILE=$1
if [ ! -f "$SRC_FILE" ]; then
    echo "Error: file not found: $SRC_FILE"
    exit 1
fi

OUT_DIR="build"
mkdir -p "$OUT_DIR"

BASE_NAME=$(basename "$SRC_FILE" .c)
OUT_FILE="$OUT_DIR/$BASE_NAME"

gcc "$SRC_FILE" -o "$OUT_FILE" $(pkg-config --cflags --libs libdrm_amdgpu)

if [ $? -ne 0 ]; then
    echo "Compile failed."
    exit 1
fi

echo "Compile success: $OUT_FILE"

./"$OUT_FILE"