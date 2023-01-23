#include "stdio.h"
#include "stdlib.h"

#define SDL_MAIN_HANDLED
#include "include/SDL2/SDL.h"
#include "glad/glad.h"

//add meta
//add light stuff

SDL_Window* window = NULL;

int main(void){
	printf("glad: %d sdl_init:%d\n",gladLoadGL(),SDL_Init(SDL_INIT_VIDEO));
	if(!(window = SDL_CreateWindow("Cone Image", 0, 100, 800, 800, SDL_WINDOW_OPENGL)))
		printf("Failed to initilize SDL Window");
	return 0;
}
