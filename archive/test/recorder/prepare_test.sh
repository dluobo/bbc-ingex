#!/bin/sh

# make the directories
mkdir -p cache
mkdir -p pse
mkdir -p browse
mkdir -p logs

# copy config file and set absolute dir and archive dir
PWD=`pwd`
INGEX_ARCHIVE_DIR=../..
sed -e "s|ABS_DIR|$PWD|g" -e "s|INGEX_ARCHIVE_DIR|$INGEX_ARCHIVE_DIR|g" < ../../config/ingex.cfg.test > ingex.cfg

