@echo off
del "coneImage.exe"
gcc -m32 -g glad.c main.c -lopengl32 -L lib -ladlib -lcglm -lSDL2main -lSDL2 -lSDL2.dll -lSDL2_test -lSDL2_image -lSDL2_image.dll -o coneImage.exe