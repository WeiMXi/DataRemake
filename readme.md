One PC that installed ROOT can compile use:
```cpp
g++ -o dataRemake dataRemake.cpp $(root-config --libs --cflags) -O3
# just run it
./dataRemake
```