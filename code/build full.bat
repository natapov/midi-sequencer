@echo off
call "c:\Program Files (x86)\Microsoft Visual Studio\2019\BuildTools\VC\Auxiliary\Build\vcvars64.bat" > nul
mkdir ..\build 2> nul
pushd ..\build
cl /Zi /I ..\dependencies\ /I ..\dependencies\imgui /I ..\dependencies\fluidsynth\include ..\code\*.cpp ..\dependencies\*.lib ..\dependencies\imgui\*.cpp Gdi32.lib Shell32.lib Ole32.lib /nologo /link /NOLOGO /NODEFAULTLIB:LIBCMT && .\sequencer.exe
popd