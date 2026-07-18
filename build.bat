@echo off
REM Dev Hunter faithful port — 32-bit MSVC build.
call "C:\Program Files\Microsoft Visual Studio\18\Professional\VC\Auxiliary\Build\vcvars32.bat" >nul
cl /nologo /O2 /fp:precise /W3 /D_CRT_SECURE_NO_WARNINGS ^
   main.c helpers.c init.c tick.c render.c entity.c spawn.c render_cars.c glow.c /Fe:devhunter.exe ^
   /link user32.lib gdi32.lib winmm.lib
