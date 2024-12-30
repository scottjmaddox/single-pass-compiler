#!/bin/sh
set -ex

clang -Weverything -Wno-gnu-case-range -Wno-poison-system-directories -o main main.c
