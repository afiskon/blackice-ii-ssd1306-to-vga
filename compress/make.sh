#!/bin/sh

set -e

gcc -Wall encode.c -o encode
gcc -Wall decode.c -o decode

gcc -Wall encode2.c -o encode2
gcc -Wall decode2.c -o decode2
