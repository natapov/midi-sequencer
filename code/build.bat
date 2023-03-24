@echo off
call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat" > nul
mkdir ..\build 2> nul
pushd ..\build
cl /Zi /I ..\dependencies\ /I ..\dependencies\imgui ..\code\*.cpp ..\dependencies\*.lib imgui*.obj Gdi32.lib Shell32.lib Ole32.lib opengl32.lib Dwmapi.lib Comdlg32.lib /nologo /link /NOLOGO /NODEFAULTLIB:LIBCMT && .\sequencer.exe
popd