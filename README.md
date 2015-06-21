# fanblaster
blasts your nvidia gpu fans at max.  

you don't overclock, but you're worried about your fan speed?
something keeps setting your fans back to a low-speed profile and crashing your gpu?

this will cover it for you.

plays an annoying sound if a thermal reading increases above a limit passed on the command line

disclaimer: i don't really code for windows, but... "works on my machine!" (*cringe*)

requires that you have a copy of nvapi.dll somewhere on your machine (which will be the case for everyone with one of these gpus, i imagine)

built on windows with mingw:

> windres fanblaster.rc -O coff -o fanblaster.res

> g++ fanblaster.cc fanblaster.res -o fanblaster.exe -lwinmm -static-libgcc -static-libstdc++

example usage to alarm if temp goes above 65C:

> fanblaster.exe 65
