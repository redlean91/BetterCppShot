@echo off

TITLE "BetterCppShot Compiling - x86 (x32)"

for /f "usebackq eol=# tokens=1,* delims==" %%A in (".env") do (
    set %%A=%%B
)

cmake -B build32 ^
  -G "MinGW Makefiles" ^
  -DCMAKE_C_COMPILER="%GCC32%/bin/gcc.exe" ^
  -DCMAKE_CXX_COMPILER="%GCC32%/bin/g++.exe" ^
  -DCMAKE_RC_COMPILER="%GCC32%/bin/windres.exe"

cmake --build build32

echo All done!
timeout 3