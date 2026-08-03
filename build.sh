#!/bin/bash

CC=g++
OPTS="-std=c++23 -Wall -Wextra -Wsign-conversion -fhardened -Wno-hardened -pedantic 
-fno-exceptions -g -O0 -fsanitize=address"
LINK="-lstdc++exp"
INCLUDE=-I../code/

mkdir build 2> /dev/null
cd build
CMD="$CC ../unity.cpp ../code/libtommath.cpp $OPTS $INCLUDE -o compiler $LINK"
echo $CMD
$CMD
cd ..