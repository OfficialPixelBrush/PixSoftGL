#LD_PRELOAD=./build/libGL.so ./tests/testCube
#LD_PRELOAD=./build/libGL.so ./tests/testSuite
#gdb --args env LD_PRELOAD=./build/libGL.so ./tests/testSuite
# Classicube must either be lied to or compiled with OpenGL 1.1 support
mkdir cc; cd cc
LD_PRELOAD=../build/libGL.so ~/ClassiCube/ClassiCube
cd ..
