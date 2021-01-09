#!/bin/bash

make -f Makefile_ios
echo "make done..."

cp libVideoRtc.a ../../McpttRtc/McpttRtc/Media/libs
echo "copy libVideoRtc.a done..."
