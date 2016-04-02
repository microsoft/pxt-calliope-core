#!/bin/sh

set -x
set -e
mkdir -p build
yotta target bbc-microbit-classic-gcc
yotta update
yotta build
