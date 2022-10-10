@echo off
call "c:\Program Files (x86)\Microsoft Visual Studio\2019\BuildTools\VC\Auxiliary\Build\vcvars64.bat" > nul
mkdir ..\build 2> nul
pushd ..\build
cl -Zi /I ..\dependencies\ /I ..\dependencies\imgui ..\code\*.cpp ..\dependencies\*.lib imgui.obj imgui_demo.obj imgui_draw.obj imgui_impl_glfw.obj imgui_impl_opengl3.obj imgui_tables.obj imgui_widgets.obj  OpenGL32.lib Gdi32.lib Shell32.lib User32.lib msvcrt.lib msvcmrt.lib Ole32.lib /nologo /link /NOLOGO /NODEFAULTLIB:LIBCMT && .\sequencer.exe
popd