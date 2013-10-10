@echo off

:: This builds using MinGW-w64 for 32 and 64 bit (http://mingw-w64.sourceforge.net/)
:: Make sure both mingw-w32\bin and mingw-w64\bin are in the PATH

:: -march=core2
set FLAGS=-mconsole -static-libgcc -static-libstdc++ -O3 -Wall -Werror -s

echo Compiling 32-bit...
i686-w64-mingw32-g++ %FLAGS% -o objcompress.exe objcompress.cc
i686-w64-mingw32-g++ %FLAGS% -D MINI_JS -o objcompress-mini.exe objcompress.cc
:: i686-w64-mingw32-g++ %FLAGS% -o objanalyze.exe objanalyze.cc
i686-w64-mingw32-g++ %FLAGS% -o objnormalize.exe objnormalize.cc

echo.

echo Compiling 64-bit...


x86_64-w64-mingw32-g++ %FLAGS% -o objcompress64.exe objcompress.cc
x86_64-w64-mingw32-g++ %FLAGS% -D MINI_JS -o objcompress64-mini.exe objcompress.cc
:: x86_64-w64-mingw32-g++ %FLAGS% -o objanalyze64.exe objanalyze.cc
x86_64-w64-mingw32-g++ %FLAGS% -o objnormalize64.exe objnormalize.cc

pause
