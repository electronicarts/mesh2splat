@echo off
if exist build rmdir /s /q build
if exist bin rmdir /s /q bin

@echo on
mkdir build
cd build
cmake ..        
cmake --build .
cd ..
pause
