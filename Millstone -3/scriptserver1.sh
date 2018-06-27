#!/bin/bash
SCRIPT=$(readlink -f "$0")
SCRIPTPATH=$(dirname "$SCRIPT")
SCRIPTPATH="$SCRIPTPATH/database.db"
./main -p 55443 -d $SCRIPTPATH