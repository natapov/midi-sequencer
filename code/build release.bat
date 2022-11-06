@echo off
call "c:\Program Files (x86)\Microsoft Visual Studio\2019\BuildTools\VC\Auxiliary\Build\vcvars64.bat" > nul
mkdir ..\build 2> nul
pushd ..\build
cl /O2 /I ..\dependencies\ /I ..\dependencies\imgui /I ..\dependencies\fluidsynth\include ..\dependencies\fluidsynth\lib\*.a ..\code\*.cpp ..\dependencies\*.lib ..\dependencies\imgui\*.cpp Gdi32.lib Shell32.lib Ole32.lib /nologo /link /NOLOGO /NODEFAULTLIB:LIBCMT 
mkdir ..\release 2> nul
copy sequencer.exe ..\release\sequencer.exe
popd