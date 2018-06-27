#!/bin/bash
SCRIPT=$(readlink -f "$0")
./client -p $1 -h "127.0.0.1" -T