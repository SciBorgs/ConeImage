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
	SDL_Event* even_t = calloc(sizeof(SDL_Event), 1);
	while(1)
	while (SDL_PollEvent(even_t)){
				switch(even_t->type){
								case SDL_QUIT:
												return 0;
								break;		
				}
  }
	return 0;
}
