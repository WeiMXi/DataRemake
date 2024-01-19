One PC that installed ROOT can compile use:
```bash
vim dataReMake.cpp # change IS_MACRO to false
g++ -o dataRemake dataRemake.cpp $(root-config --libs --cflags) -O3
# just run it
./dataRemake
```