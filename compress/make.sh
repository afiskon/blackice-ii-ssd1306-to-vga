#!/bin/sh

set -e

gcc -Wall encode.c -o encode
gcc -Wall decode.c -o decode
