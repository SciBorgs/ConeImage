@echo off
gcc -m32 glad.c main.c -L lib -lSDL2main -lSDL2 -lSDL2.dll -lSDL2_test -o coneImage.exe