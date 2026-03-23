# BetterCppShot
Originally from [Cvolton](https://github.com/Cvolton/CppShot), made """better""" by Redlean

All credits go to Cvolton for the original code, i'm just simply doing this as a chance to learn a bit of C++.

A transparent screnshot utility written in C++. Tested for compatibility on Windows XP onwards. (working to make it work on 98/2000)

## Building notes
The build environment uses CMake.

### x86/x64 - TDM-GCC 5.1
This is currently the latest compiler I've been able to find that supports Windows 98 and 2000. Note that this compiler doesn't support features beyond C++14. Available for download [here](https://sourceforge.net/projects/tdm-gcc/files/TDM-GCC%20Installer/).

## Compiling
```
mkdir build
cd build
cmake -G "MinGW Makefiles" ..
mingw32-make
```