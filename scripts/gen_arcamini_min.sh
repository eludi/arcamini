#!/bin/sh
BASEDIR=$(dirname "$0")
uglifyjs $BASEDIR/../browser_runtime/graphicsGL.js $BASEDIR/../browser_runtime/audio.js $BASEDIR/../browser_runtime/app.js --mangle --compress -O ascii_only=true
