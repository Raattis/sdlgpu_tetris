# SDL_gpu tetris
A hello world application. Based on the `testgpu_spinning_cube.c` from SDL examples. Modified to use SDL_AppInit, SDL_AppEvent, SDL_AppIterate and SDL_AppQuit callbacks and no global state. Uses cmake FetchContent to download and configure SDL. Links SDL statically to produce a single portable executable.

<img src="https://github.com/user-attachments/assets/5fa1e483-6b44-43bb-a5f0-50178bfb35b8" width="100" style="float: right; display: block; margin: auto;">

## Compiling
### Visual Studio 2022 with cmake
 * only requires Visual Studio 2022
 * right-click folder background, select open with visual studio
 * wait for cmake configuration to finish
 * compile and run
### Visual Studio 2022 solution
 * only requires Visual Studio 2022
 * start "x64 Native Tools Command Prompt for Visual Studio 2022"
 * navigate to this folder, run "cmake -S . -B build"
 * go to the newly created build folder and open the .sln file
 * compile and run normally
### Linux & Mac
 * cmake -S . -B build
 * cd build
 * make
