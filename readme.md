## Run as an ROOT macro
`root -q dataRemake.cpp`
## Compile
One PC that installed ROOT can compile use:
```bash
vim dataReMake.cpp # change IS_MACRO to false
g++ -o dataRemake dataRemake.cpp $(root-config --libs --cflags) -O3
# just run it
./dataRemake
```

## Skip some channels
One can add any no "U" or "D" symbol at the beginning of the corresponding line, then the two channel will be skipped.

For example:
```text
D,1,1,296,323
#D,1,2,297,330
#D,1,3,295,320
```

```text
user@PC:~/MyCodes/dataremake$ root -q dataRemake.cpp 
Processing dataRemake.cpp...
Program Started!
----------------------------------------------------
2 #D,1,2,297,330 is skipped!
3 #D,1,3,295,320 is skipped!
Reading Mapping2Detector.csv finished!
----------------------------------------------------
Sorting ...
[==================================================] 100 % ☀ 🎓
Writing remain data to 0111test_tunning01_single OUTPUT.root ...
[==================================================] 100 % ☀ 🎓
RealTime: 0.98 s, CpuTime: 0.97 s
(int) 0

```