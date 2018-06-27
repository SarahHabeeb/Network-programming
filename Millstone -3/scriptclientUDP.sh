#!/bin/bash
SCRIPT=$(readlink -f "$0")
./client -p 55443 -h "127.0.0.1" -U