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
- **Direct window capture** — no more full-screen black/white backdrops on modern Windows:
  - Windows 10 1803+: the window itself is captured through Windows.Graphics.Capture (real per-pixel alpha, rounded corners, works on DirectComposition apps like Windows Terminal, the taskbar and the Action Center), and the window's original DWM drop shadow is recovered with a brief hide-and-show of the window (~150 ms, no backdrops, nothing synthesized).
  - Windows 2000+: falls back to a direct PrintWindow capture.
  - Anything older (98/ME/XP): falls back to the classic black/white backdrop method.
  - Can be toggled in Settings ("Direct Capture"); shadow recovery can be disabled by setting `CaptureShadow` to `0` under `HKCU\SOFTWARE\CppShot`.

## Building notes
The build environment uses CMake.

### x86/x64 - TDM-GCC 5.1
This is currently the latest compiler I've been able to find that supports Windows 98 and 2000. Note that this compiler doesn't support features beyond C++14. Available for download [here](https://sourceforge.net/projects/tdm-gcc/files/TDM-GCC%20Installer/).

## Compiling
I have included 2 premade scripts to compile:

- **build64.bat**
- **build32.bat**

You will need to create a **.env** file, you can use the **.envExample** as a template, but replace it with your actual TDM-GCC paths.