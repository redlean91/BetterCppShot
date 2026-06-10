@echo off

TITLE "BetterCppShot Compiling - x64"

for /f "usebackq eol=# tokens=1,* delims==" %%A in (".env") do (
    set %%A=%%B
)

cmake -B build64 ^
  -G "MinGW Makefiles" ^
  -DCMAKE_C_COMPILER="%GCC64%/bin/gcc.exe" ^
  -DCMAKE_CXX_COMPILER="%GCC64%/bin/g++.exe" ^
  -DCMAKE_RC_COMPILER="%GCC64%/bin/windres.exe"

cmake --build build64

echo All done!
timeout 3