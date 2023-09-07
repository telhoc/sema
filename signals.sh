#!/bin/bash
gcc -ggdb -o receiver receiver.c -lrt
gcc -ggdb -o rthread receiver_thread.c -lrt -lpthread
gcc -ggdb -o sender sender.c -lrt
gcc -ggdb -o timer_rx timer_process.c -lrt -lpthread

