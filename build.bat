@echo off
REM Dev Hunter faithful port — 32-bit MSVC build.
call "C:\Program Files\Microsoft Visual Studio\18\Professional\VC\Auxiliary\Build\vcvars32.bat" >nul
cl /nologo /O2 /fp:precise /W3 /D_CRT_SECURE_NO_WARNINGS ^
   main.cpp helpers.cpp init.cpp tick.cpp render.cpp entity.cpp spawn.cpp render_cars.cpp glow.cpp /Fe:devhunter.exe ^
   /link user32.lib gdi32.lib winmm.lib
