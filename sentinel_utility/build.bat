@echo off
REM Richiede MinGW-w64 (es. "choco install mingw"). Produce un exe standalone, senza DLL.
g++ -std=c++17 -O2 -s -static -mwindows sentinel.cpp -o sentinel.exe -lcomctl32 -lshell32
if errorlevel 1 exit /b 1
sentinel.exe --test
