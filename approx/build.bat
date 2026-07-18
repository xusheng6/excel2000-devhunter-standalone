@echo off
REM Dev Hunter standalone — 32-bit build with MSVC.
call "C:\Program Files\Microsoft Visual Studio\18\Professional\VC\Auxiliary\Build\vcvars32.bat" >nul
cl /nologo /O2 /fp:precise /W3 /D_CRT_SECURE_NO_WARNINGS ^
   main.c game.c /Fe:devhunter.exe ^
   /link user32.lib gdi32.lib winmm.lib
