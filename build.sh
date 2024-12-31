#!/bin/sh
set -ex

clang -std=c99 -pedantic -Wall -Wno-gnu-case-range -o main main.c
