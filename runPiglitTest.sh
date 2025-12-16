#!/bin/bash
cd ../piglit
export LIBGL_DRIVERS_PATH=/home/torben/PixSoftGL/build
export LD_PRELOAD=/home/torben/PixSoftGL/build/libGL.so
export LD_LIBRARY_PATH=/home/torben/PixSoftGL/build:$LD_LIBRARY_PATH
./piglit run tests/spec/quick.py -t gl-1.0 results/opengl10 -p glx
./piglit summary html summary/opengl10 results/opengl10
xdg-open summary/opengl10/index.html
cd ../PixSoftGL

