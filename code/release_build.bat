@echo off
call "c:\Program Files (x86)\Microsoft Visual Studio\2019\BuildTools\VC\Auxiliary\Build\vcvars64.bat" > nul
mkdir ..\build
pushd ..\build
cl /O2 /I ..\dependencies\ /I ..\dependencies\imgui /I ..\dependencies\irrKlang\include ..\code\*.cpp ..\dependencies\*.lib ..\dependencies\*.exp ..\dependencies\imgui\imgui*.cpp OpenGL32.lib Gdi32.lib Shell32.lib User32.lib msvcrt.lib msvcmrt.lib && .\sequencer.exe

popd