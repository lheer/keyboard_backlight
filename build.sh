#!/bin/sh

set -e

# Debug
g++ main.cpp devices.cpp -o keyboard_backlight_d -I. -lfmt -ggdb3 -DDEBUG

# Release
g++ main.cpp devices.cpp -o keyboard_backlight -I. -lfmt -O2
