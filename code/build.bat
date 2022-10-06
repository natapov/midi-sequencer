@echo off
call "c:\Program Files (x86)\Microsoft Visual Studio\2019\BuildTools\VC\Auxiliary\Build\vcvars64.bat" > nul
mkdir ..\build
pushd ..\build
cl -Zi /I ..\dependencies\ /I ..\dependencies\imgui ..\code\*.cpp ..\dependencies\*.lib ..\dependencies\*.exp ..\dependencies\imgui\imgui*.cpp OpenGL32.lib Gdi32.lib Shell32.lib User32.lib msvcrt.lib msvcmrt.lib Ole32.lib && .\sequencer.exe
popd