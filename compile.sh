# Driver
g++ -fPIC -shared main.cpp -o libGL.so

# Test Software
g++ TestApps/cube.cpp -lX11 -lGL -o cube