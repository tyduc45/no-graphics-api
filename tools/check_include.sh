#!/bin/bash
if [ $# -lt 1 ]; then
  echo "please input the source file you wanna check\n"
  exit 1
fi 
SRC_FILE=$1
gcc -H -E "$SRC_FILE" $(pkg-config --cflags libdrm_amdgpu)  2>&1 > /dev/null