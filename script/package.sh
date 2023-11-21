#!/bin/bash

THIS_DIR=$(dirname "$0")
ELECTRON_DIR=$THIS_DIR/../

electron-packager $ELECTRON_DIR --out $ELECTRON_DIR/out --overwrite --icon $ELECTRON_DIR/assets/icons/app-icon.icns
