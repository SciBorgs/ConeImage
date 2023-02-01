#include "stdio.h"
#include "stdlib.h"
#include "math.h"

#define SDL_MAIN_HANDLED
#include "include/SDL2/SDL.h"
#include "include/SDL2/SDL_image.h"
#include "include/cglm/cglm.h" //compiling this took longer than I would like to admit
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
    float nx, ny, nz; //normal vec; perp to face for lighting calculation
};

struct Facet
{
    float normal[3];
    float v1[3];
    float v2[3];
    float v3[3];
};

extern void UpdateLight(vec3 pos, vec3 color, GLuint pog);
extern char* ReadBytes(char* path);
extern void ScreenShot();
extern unsigned int* ReadSTLFile(const char* filename);
extern SDL_Surface* flip_surface(SDL_Surface* surface);

char* const PATH_TO_META = "output/meta", *const PATH_TO_OUT = "output/framebuf", *const PATH_FRAGMENT_SHADER = "shaders/frag.shader", *const PATH_VERTEX_SHADER = "shaders/vert.shader";

unsigned char* pixels; //will increase performance if we only init this once
struct GLVertex* cone = NULL;
unsigned int coneSize = 0;
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
  unsigned int* ptr = ReadSTLFile("cone.STL");
	cone = ptr[1];
	coneSize = ptr[0];
	free(ptr);//allocated on heap
  GLuint program = glCreateProgram();
	{//compile shaders and enable-- boilerplate
				char* tmp = ReadBytes(PATH_VERTEX_SHADER);
				GLuint vertexShader  = glCreateShader(GL_VERTEX_SHADER);
				glShaderSource(vertexShader, 1, &tmp, NULL);
				glCompileShader(vertexShader);
				free(tmp); //no jem leaks
				tmp = ReadBytes(PATH_FRAGMENT_SHADER);
				GLuint fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
				glShaderSource(fragmentShader, 1, &tmp, NULL);
				glCompileShader(fragmentShader);
				free(tmp);
				glAttachShader(program, vertexShader);
				glAttachShader(program, fragmentShader);
					
				glLinkProgram(program);
				glUseProgram(program);
	}
	//set uniform up
  mat4 model, view, projection; //model is cone rot and pos, view is camera angle, projection is perspective for this
	glm_mat4_identity(model); glm_mat4_identity(view); glm_mat4_identity(projection);
	glm_translate_make(model,(vec3){0,-100,0});
	vec3 zenith, zero, up;
	{ //new stack frame because of tmp vars
  glm_vec3_zero(zero); glm_vec3_zero(zenith); glm_vec3_zero(up);
  zenith[2] = -1;
	printf("{%f,%f,%f}\n", zenith[0], zenith[1], zenith[2]);
  up[1] = 1; //up vec
	glm_perspective(1.22173, WINDOW_WIDTH / WINDOW_HEIGHT, 0.2, 50000, projection);
  //glm_ortho(0,WINDOW_WIDTH,0,WINDOW_HEIGHT,0.2,5000,projection);
	glm_look(zero, zenith, up, view);
	}
	
  GLuint locModel = 0, locView = 0, locProjection = 0;
  locModel = glGetUniformLocation(program, "model");
  locView = glGetUniformLocation(program, "view");
  locProjection = glGetUniformLocation(program, "projection");
	glUniformMatrix4fv(locModel,1,GL_FALSE,model);
  glUniformMatrix4fv(locView,1,GL_FALSE,view);
  glUniformMatrix4fv(locProjection,1,GL_FALSE,projection);

	UpdateLight((vec3){0,0,0},(vec3){0.1,0.1,2.5},program);
	
	glEnable(GL_DEPTH_TEST);  //so that 3d works like you would expect
  glDepthFunc(GL_LESS);
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
	
	/*clientBuffer = calloc(sizeof(struct GLVertex),3); //allocate on heap just to be safe
	clientBuffer[0] = (struct GLVertex){0,0,0, 1,0,0, 0,0,1}; //x,y,z   r,g,b,	  n.x, n.y, n.z
	clientBuffer[1] = (struct GLVertex){1,0,0, 1,0,0, 0,0,1};
	clientBuffer[2] = (struct GLVertex){1,1,0, 1,0,0, 0,0,1}; //debug*/
	
	glBufferData(GL_ARRAY_BUFFER, coneSize * 3 * sizeof(struct GLVertex), cone, GL_DYNAMIC_READ); //we will use this to store the cone's model w/ color to render

   //vertex boiler plate-- set up that GLVertex struct with gl
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(struct GLVertex), NULL); //x y z part
	glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(struct GLVertex), 3 * sizeof(float)); //r g b part
  glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, sizeof(struct GLVertex), 6 * sizeof(float)); //n.x n.y n.z part
	glEnableVertexAttribArray(0);
	glEnableVertexAttribArray(1);
  glEnableVertexAttribArray(2);

	//glClearColor(0,1,0,1); //for testing
	SDL_Event* even_t = calloc(sizeof(SDL_Event), 1);
	const Uint8 *state = SDL_GetKeyboardState(NULL);
	float offx = 0, offy = 0, offz = 0;
	while(1){
				glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
				//actual rendering happens here
				glDrawArrays(GL_TRIANGLES, 0, coneSize * 3);
				SDL_GL_SwapWindow(window);
				while (SDL_PollEvent(even_t)){
				switch(even_t->type){
								case SDL_QUIT:
												printf("GL ERROR: %d\n",glGetError());
												ScreenShot(frameBufferTexture);
												return 0;
								break;		
				}
				if(state[SDL_SCANCODE_W])
								zenith[1] += 0.025;
				if(state[SDL_SCANCODE_S])
								zenith[1] -= 0.025;
				if(state[SDL_SCANCODE_D])
								zenith[0] += 0.025;
				if(state[SDL_SCANCODE_A])
								zenith[0] -= 0.025;
				if(state[SDL_SCANCODE_S] || state[SDL_SCANCODE_W] || state[SDL_SCANCODE_A] || state[SDL_SCANCODE_D]){
								glm_look(zero, zenith, up, view);
								glUniformMatrix4fv(locView,1,GL_FALSE,view);
				}
				if(state[SDL_SCANCODE_LCTRL]){
				    if(state[SDL_SCANCODE_LSHIFT]){
				    if(state[SDL_SCANCODE_X])
										offx += 2.5;
						if(state[SDL_SCANCODE_Y])
										offy += 2.5;
				    if(state[SDL_SCANCODE_Z])
										offz += 2.5;
				    }else{
				    if(state[SDL_SCANCODE_X])
										offx -= 2.5;
						if(state[SDL_SCANCODE_Y])
										offy -= 2.5;
				    if(state[SDL_SCANCODE_Z])
										offz -= 2.5;
						}
				    glm_translate_make(model,(vec3){0 + offx,-100 + offy,0 + offz});
				    glUniformMatrix4fv(locModel,1,GL_FALSE,model);
				}
  }
	}
	return 0;
}

//mat4 ViewMatrixRotate(

void UpdateLight(vec3 pos, vec3 color, GLuint pog){
    GLuint locColor = 0, locPos = 0;
    locPos = glGetUniformLocation(pog, "lightPos");
    locColor = glGetUniformLocation(pog, "lightColor");
    glUniform3f(locPos, pos[0], pos[1], pos[2]);
    glUniform3f(locColor, color[0], color[1], color[2]);
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
//CONSIDER: running this on a seperate thread(s) because file operations can be cumbersome
void ScreenShot(){
    int depth = 8 + 8 + 8 + 8, /*8 red bits, 8 green bits, 8 blue bits, 8 alpha bits*/ pitch = (depth / 8) * WINDOW_WIDTH; //pitch is in bytes so div depth by 8
    glCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 0, 0, WINDOW_WIDTH, WINDOW_HEIGHT);
    glGetTexImage(GL_TEXTURE_2D, 0, GL_BGRA, GL_UNSIGNED_BYTE, pixels);
    //char name[512] = PATH_TO_OUT;
    if(IMG_SavePNG(flip_surface(SDL_CreateRGBSurfaceFrom(pixels, WINDOW_WIDTH, WINDOW_HEIGHT, depth, pitch, 0, 0, 0, 0)), "output/framebuf/test.png"))
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
unsigned int* ReadSTLFile(const char* filename){
				char* bytes = ReadBytes(filename);
				int i = 80, srt = i + 4 - 1; //we start reading at byte 80 to skip needless header
				char* integer = calloc(sizeof(char),4);
				integer[0] = bytes[i]; //memcpy? what is that? -- Nathanael
				integer[1] = bytes[i + 1];
				integer[2] = bytes[i + 2];
				integer[3] = bytes[i + 3];
				unsigned int sz = *((unsigned int*)integer);
				printf("%d\n",sz);
				free(integer);
				struct Facet* out = calloc(sizeof(struct Facet), sz);
				struct GLVertex* actualOut = calloc(sizeof(struct GLVertex), sz * 3);
				for(i = srt; (i - srt) < (sz * 50); i += 50){
								struct Facet* cur = out + (i - srt) / 50;
								printf("before: {%f,%f,%f}\n",cur->v1[0],cur->v1[1],cur->v1[2]);
								int stride = sizeof(float) * 3;
								//memcpy(cur,bytes + i, sizeof(struct Facet));
								//printf("after: {%f,%f,%f}\n",cur->v1[0],cur->v1[1],cur->v1[2]);
								int cnt = 0;
								{
								LOAD:
								asm("NOP");
								int init = i + cnt * stride;
								for(int offset = init; (offset - init) < stride; offset += sizeof(float)){
								for(int bytenum = 0; bytenum < sizeof(float); bytenum++)
												((char*)(cur->normal + (offset - i) / sizeof(float)))[bytenum] = *(bytes + offset + bytenum);
								}}
								if(cnt < 3){cnt++; goto LOAD;}
				}
				free(bytes);
				for(i = 0; i < sz; ++i){
				struct Facet cur = out[i];
				
				actualOut[i].x = cur.v1[0] / 100000.0;
				actualOut[i].y = cur.v1[1] / 100000.0;
				actualOut[i].z = cur.v1[2] / 100000.0;
								
				actualOut[i + 1].x = cur.v2[0] / 100000.0;
				actualOut[i + 1].y = cur.v2[1] / 100000.0;
				actualOut[i + 1].z = cur.v2[2] / 100000.0;
								
				actualOut[i + 2].x = cur.v3[0] / 100000.0;
				actualOut[i + 2].y = cur.v3[1] / 100000.0;
				actualOut[i + 2].z = cur.v3[2] / 100000.0;
								
				for(int j = 0; j < 3; j++){
				actualOut[i + j].nx = cur.normal[0];
				actualOut[i + j].ny = cur.normal[1];
				actualOut[i + j].nz = cur.normal[2];
				
				actualOut[i + j].r = 1;
				actualOut[i + j].g = 0;
				actualOut[i + j].b = 0;		
				}}
				//welcome to C
				unsigned int* a = calloc(8,1);
				a[0] = sz;
				a[1] = actualOut;
				return a;
}
