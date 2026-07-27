#!/bin/sh
BASEDIR=$(dirname "$0")
uglifyjs $BASEDIR/graphicsGL.js $BASEDIR/audio.js $BASEDIR/app.js --mangle --compress -O ascii_only=true
