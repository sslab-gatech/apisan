#!/bin/bash
DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
APISAN_DIR=$DIR/..
PYTHONPATH=$APISAN_DIR:$PYTHONPATH $DIR/test.py
