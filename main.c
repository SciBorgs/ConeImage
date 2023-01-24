#include "stdio.h"
#include "stdlib.h"

#define SDL_MAIN_HANDLED
#include "include/SDL2/SDL.h"
#include "include/SDL2/SDL_image.h"
#include "glad/glad.h"

//add meta
//add light stuff
#define WINDOW_WIDTH 800
#define WINDOW_HEIGHT 800
#define WINDOW_SPECIAL_OFFSET 100 //this is for me since I keep my taskbar at the top so I need to make sure the window doesn't generate in the taskbar - Nathanael

//this is a model of each "vertex" that will be passed to the vertex shader, makes life easier when setting up with the glVertexAttribPointer proc
struct GLVertex
{
    float x, y, z;        // Vertex
    float r, g, b;     // Color (for cone since it's simplier like this when I random gen)
};

char* ReadBytes(char* path);
void ScreenShot();
SDL_Surface* flip_surface(SDL_Surface* surface);

char* const PATH_TO_META = "output/meta", *const PATH_TO_OUT = "output/framebuf", *const PATH_FRAGMENT_SHADER = "shaders/frag.shader", *const PATH_VERTEX_SHADER = "shaders/vert.shader";

unsigned char* pixels; //will increase performance if we only init this once

struct GLVertex* clientBuffer; //update this to update GL_ELEMENTS_BUFFER
//TODO: perspective, view, and model math
SDL_Window* window = NULL;
SDL_GLContext glContext = NULL;
int main(void){
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_COMPATIBILITY);
	printf("sdl_init:%d\n",SDL_Init(SDL_INIT_VIDEO));
  if(!(window = SDL_CreateWindow("Cone Image", 0, WINDOW_SPECIAL_OFFSET, WINDOW_WIDTH, WINDOW_HEIGHT, SDL_WINDOW_OPENGL ))){
		printf("Failed to initilize SDL Window");
	}
	if(!(glContext = SDL_GL_CreateContext(window)))
    printf("Failed to initilize opengl context");;
  if(!(gladLoadGL())) //NOTE TO SELF: NEVER PUT THIS BEFORE SDL_GL_CreateContext; THREE HOURS DEBUGGING
    printf("Failed to initilize opengl");
	
	{//compile shaders and enable-- boilerplate
				GLuint program = glCreateProgram();
				char* tmp = ReadBytes(PATH_VERTEX_SHADER);
				GLuint vertexShader  = glCreateShader(GL_VERTEX_SHADER);
				glShaderSource(vertexShader, 1, &tmp, NULL);
				glCompileShader(vertexShader);
				tmp = ReadBytes(PATH_FRAGMENT_SHADER);
				GLuint fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
				glShaderSource(fragmentShader, 1, &tmp, NULL);
				glCompileShader(fragmentShader);
					
				glAttachShader(program, vertexShader);
				glAttachShader(program, fragmentShader);
					
				glLinkProgram(program);
				glUseProgram(program);
	}
	//gen custom texture buffer to use for "screenshots"
	GLuint frameBufferTexture;
	glGenTextures(1, &frameBufferTexture);
	glBindTexture(GL_TEXTURE_2D, frameBufferTexture);
	
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, WINDOW_WIDTH, WINDOW_HEIGHT, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);  
	
	pixels = calloc(sizeof(char),WINDOW_WIDTH * WINDOW_HEIGHT * 4); //for use later
  //get our arraybufs
	GLuint ARRAYS_BUF = 0;
	glGenBuffers(1, &ARRAYS_BUF);
	glBindBuffer(GL_ARRAY_BUFFER, ARRAYS_BUF); //prob never gonna be unbound since we keeping it simple
	
	clientBuffer = calloc(sizeof(struct GLVertex),3); //allocate on heap just to be safe
	clientBuffer[0] = (struct GLVertex){0,0,0, 1,0,0}; //x,y,z   r,g,b
	clientBuffer[1] = (struct GLVertex){0.1,0,0, 1,0,0};
	clientBuffer[2] = (struct GLVertex){0.1,0.1,0, 1,0,0};
	
	glBufferData(GL_ARRAY_BUFFER, 3 * sizeof(struct GLVertex), clientBuffer, GL_DYNAMIC_READ); //we will use this to store the cone's model w/ color to render

   //vertex boiler plate-- set up that GLVertex struct with gl
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(struct GLVertex), NULL); //x y z part
	glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(struct GLVertex), 3 * sizeof(float)); //r g b part
	glEnableVertexAttribArray(0);
	glEnableVertexAttribArray(1);

	glClearColor(0,1,0,1); //cool green
	SDL_Event* even_t = calloc(sizeof(SDL_Event), 1);
	while(1){
				glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
				//actual rendering happens here
				glDrawArrays(GL_TRIANGLES, 0, 3);
				SDL_GL_SwapWindow(window);
				while (SDL_PollEvent(even_t)){
				switch(even_t->type){
								case SDL_QUIT:
												ScreenShot(frameBufferTexture);
												return 0;
								break;		
				}
  }
	}
	return 0;
}

char* ReadBytes(char* path){
    FILE *fp = fopen(path, "r");
    if (fp == NULL)
    {
        printf("Error: could not open file %s", path);
				return NULL;
    }
    fseek(fp, 0L, SEEK_END);
    unsigned long numbytes = ftell(fp);
    fseek(fp, 0L, SEEK_SET);  
    // read one character at a time and concat
    char ch, *out = calloc(sizeof(char),numbytes + 1);
		int i;
    for(i = 0; (ch = fgetc(fp)) != EOF; ++i)
        out[i] = ch;
		out[i] = '\0'; //just to be safe
    // close the file
    fclose(fp);
		return out;
}

void ScreenShot(){
    int depth = 8 + 8 + 8 + 8, /*8 red bits, 8 green bits, 8 blue bits, 8 alpha bits*/ pitch = (depth / 8) * WINDOW_WIDTH; //pitch is in bytes so div depth by 8
    glCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 0, 0, WINDOW_WIDTH, WINDOW_HEIGHT);
    glGetTexImage(GL_TEXTURE_2D, 0, GL_BGRA, GL_UNSIGNED_BYTE, pixels);
    //char name[512] = PATH_TO_OUT;
    if(!IMG_SavePNG(flip_surface(SDL_CreateRGBSurfaceFrom(pixels, WINDOW_WIDTH, WINDOW_HEIGHT, depth, pitch, 0, 0, 0, 0)), "output/framebuf/test.png"))
				printf("Failed to save PNG\n");
}
//At long last... it actually works -- Nathanael
SDL_Surface* flip_surface(SDL_Surface* surface) 
{
    SDL_LockSurface(surface);
    
    int pitch = surface->pitch; // row size
    char* temp = calloc(sizeof(char),pitch); // intermediate buffer
    char* pixels = (char*) surface->pixels;
    
    for(int i = 0; i < surface->h / 2; ++i) {
        // get pointers to the two rows to swap
        char* row1 = pixels + i * pitch;
        char* row2 = pixels + (surface->h - i - 1) * pitch;
        
        // swap rows
        memcpy(temp, row1, pitch);
        memcpy(row1, row2, pitch);
        memcpy(row2, temp, pitch);
    }
    
    free(temp);

    SDL_UnlockSurface(surface);
		return surface;
}
