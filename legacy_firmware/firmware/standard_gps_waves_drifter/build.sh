#!/bin/sh

arduino-cli --config-dir ~/embedded/wave-buoy/OpenMetBuoy-v2021a/Arduino compile -v -b SparkFun:apollo3:amap3micromod . --build-path build --output-dir output
