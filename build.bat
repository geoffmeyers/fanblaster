windres fanblaster.rc -O coff -o fanblaster.res
g++ fanblaster.cc fanblaster.res -o fanblaster.exe -lwinmm -static-libgcc -static-libstdc++

