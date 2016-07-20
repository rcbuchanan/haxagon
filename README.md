## Super Hexagon AI


To build:
```
gcc -shared -fPIC *.c -o prog.so -ldl -lm -lX11 -Wall -pedantic
```

To run:
```
LD_PRELOAD=./x86_64/prog.so LD_LIBRARY_PATH=./x86_64 ./x86_64/superhexagon.x86_64 
```

No idea if it'll work on other computers, but it works for me.

### Quick Tour of Code
`haxagon.c`: Most interesting stuff is here. There are three stages in order of execution (reverse of their order in `haxagon.c`):

1. Obtain triangle data for the frame -- before the Super Hexagon binary calls OpenGL API, run custom code to check if the API call was made from one of the injection points. If it is, process it in step 2. After this custom code is run, pass the call on to OpenGL so the game works as intended.
2. Process triangle data for the frame -- figures out which set of left/right moves the player can make to best avoid obstacles
3. Move the player -- based on the optimal path for in step 2, send a Left/Right keystroke to the frontmost window.

`vec2.c`: Small header library for working with 2d vectors.

`svg.c`, `svg.h`, `fakesvg.h`: These are for saving SVGs with debug data. `fakesvg.h` is a dummy version of `svg.h` that disables SVG outputs to avoid extra IO.
