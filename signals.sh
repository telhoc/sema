#!/bin/bash
gcc -ggdb -o receiver receiver.c -lrt
gcc -ggdb -o sender sender.c -lrt
