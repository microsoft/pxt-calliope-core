#!/bin/sh

set -x
set -e
mkdir -p build
yotta target bbc-microbit-classic-gcc
yotta update
node scripts/functionTable.js inc/BitVM.h inc/MicroBitTouchDevelop.h source/bitvm.cpp yotta_modules/microbit-dal/inc/*.h
mkdir -p ext
touch ext/config.h ext/pointers.inc ext/refs.inc
yotta build
