# BetterCppShot
A fork of [CppShot](https://github.com/Cvolton/CppShot) with multiple improvements made to it.

A transparent screnshot utility written in C++. Tested for compatibility on Windows XP onwards. (Not planning to support Windows 9x for now)

## List of added stuff/functions
- You can change screenshot path
- Instead of the default path being *"C:\test"* its now *"C:\Assets"*
- You can change keybinds
- Add delay to make sure transparency doesnt mess up when taking the screenshot
- Desktop Icons transparent screenshot taking (thanks a lot to [mrrpmeowfurry](https://github.com/mrrpmeowfurry) for helping out in bettercppshot!)
- An about menu

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