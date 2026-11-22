#!/bin/sh
# Cross-compile from Linux (needs: apt install gcc-mingw-w64-x86-64)
set -e
cd "$(dirname "$0")/src"
x86_64-w64-mingw32-windres memtable.rc -O coff -o memtable.res.o
x86_64-w64-mingw32-gcc -O2 -s -municode -mwindows memtable.c memtable.res.o \
  -o ../MemoryPeriodicTable.exe -lpsapi -ladvapi32 -lgdi32 -luser32 -lm -static
echo "Built MemoryPeriodicTable.exe"
