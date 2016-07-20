gcc -shared -fPIC *.c -o prog.so -ldl -lm -lX11 -Wall -pedantic
LD_PRELOAD=./x86_64/prog.so LD_LIBRARY_PATH=./x86_64 ./x86_64/superhexagon.x86_64 

