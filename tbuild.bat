@echo off
call "C:\Program Files\Microsoft Visual Studio\18\Professional\VC\Auxiliary\Build\vcvars32.bat" >nul
cl /nologo /O2 /fp:precise /D_CRT_SECURE_NO_WARNINGS test_render.c helpers.c render.c /Fe:test_render.exe /link user32.lib gdi32.lib >nul
