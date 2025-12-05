# Driver
g++ -fPIC -shared src/lib.cpp -lSDL3 -o libGL.so -g

# Test Software
g++ TestApps/cube.cpp -lX11 -lGL -o cube -g
g++ -std=c++17 TestApps/test.cpp -o test -lX11 -lGL -g