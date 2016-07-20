Super Hexagon AI
================

To build:
```
gcc -shared -fPIC *.c -o prog.so -ldl -lm -lX11 -Wall -pedantic
```

To run:
```
LD_PRELOAD=./x86_64/prog.so LD_LIBRARY_PATH=./x86_64 ./x86_64/superhexagon.x86_64 
```

No idea if it'll work on other computers, but it works for me.
