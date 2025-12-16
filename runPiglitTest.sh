cd ../piglit
export LD_LIBRARY_PATH=/home/torben/PixSoftGL/build:$LD_LIBRARY_PATH
LD_PRELOAD=/home/torben/PixSoftGL/build/libGL.so \
LIBGL_DEBUG=verbose glxinfo
./piglit run tests/spec/quick.py -t gl-1.0 results/opengl10
./piglit summary html summary/opengl10 results/opengl10
xdg-open summary/opengl10/index.html
cd ../PixSoftGL