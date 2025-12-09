# Driver
# Handled by CMake
#g++ -fPIC -shared src/lib.cpp -lSDL3 -o libGL.so -g
# Test Software
g++ tests/testCube.cpp -lX11 -lGL -o tests/testCube -g
g++ -std=c++17 tests/testSuite.cpp -o tests/testSuite -lX11 -lGL -g
g++ -std=c++17 tests/testPointers.cpp -o tests/testPointers -lX11 -lGL -g
