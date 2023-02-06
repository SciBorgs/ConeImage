#include "stdio.h"
#include "stdlib.h"
#include "math.h"

#define SDL_MAIN_HANDLED
#include "include/SDL2/SDL.h"
#include "include/SDL2/SDL_image.h"
#include "include/cglm/cglm.h" //compiling this took longer than I would like to admit
#include "include/adlib/stl.h"
#include "glad/glad.h"

//add meta
//add light stuff
#define WINDOW_WIDTH 800
#define WINDOW_HEIGHT 800
#define WINDOW_SPECIAL_OFFSET 100 //this is for me since I keep my taskbar at the top so I need to make sure the window doesn't generate in the taskbar - Nathanael
#define SCALE_DIV_FACTOR 10.0
#define ITER_FACTOR 5

//this is a model of each "vertex" that will be passed to the vertex shader, makes life easier when setting up with the glVertexAttribPointer proc
struct GLVertex
{
    float x, y, z;        // Vertex
    float r, g, b;     // Color (for cone since it's simplier like this when I random gen)
    float nx, ny, nz; //normal vec; perp to face for lighting calculation
};

extern void UpdateLight(vec3 pos, vec3 color, GLuint pog);
extern char* ReadBytes(char* path);
extern void ScreenShot(vec3 angle, vec3 location, vec3 color);
extern unsigned int* ReadSTLFile(const char* filename);
extern SDL_Surface* flip_surface(SDL_Surface* surface);

char* const PATH_TO_META = "output/meta/", *const PATH_TO_OUT = "output/framebuf/", *const PATH_FRAGMENT_SHADER = "shaders/frag.shader", *const PATH_VERTEX_SHADER = "shaders/vert.shader";

unsigned char* pixels; //will increase performance if we only init this once
struct GLVertex* cone = NULL;
unsigned int* coneIndices = NULL;
unsigned int coneSize = 0;

int current = 0;

//debug
//btw dont forget to disable -g before relaease!
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
	cone = ptr[0];
	coneSize = ptr[1];
    coneIndices = ptr[2];
  free(ptr); //was allocated on heap
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
	glm_translate_make(model,(vec3){-15,-15,-45});
	vec3 zenith, zero, up;
	{ //new stack frame because of tmp vars
  glm_vec3_zero(zero); glm_vec3_zero(zenith); glm_vec3_zero(up);
  zenith[2] = -1;
	printf("{%f,%f,%f}\n", zenith[0], zenith[1], zenith[2]);
  up[1] = 1; //up vec
	glm_perspective(1.22173, WINDOW_WIDTH / WINDOW_HEIGHT, 0.2, 1000, projection);
  //glm_ortho(0,WINDOW_WIDTH,0,WINDOW_HEIGHT,0.2,5000,projection);
	glm_look(zero, zenith, up, view);
	}

  GLuint locModel = 0, locView = 0, locProjection = 0, locOver = 0;
  locModel = glGetUniformLocation(program, "model");
  locView = glGetUniformLocation(program, "view");
  locProjection = glGetUniformLocation(program, "projection");
	locOver = glGetUniformLocation(program,"coneOverride");
	glUniformMatrix4fv(locModel,1,GL_FALSE,model);
  glUniformMatrix4fv(locView,1,GL_FALSE,view);
  glUniformMatrix4fv(locProjection,1,GL_FALSE,projection);

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
   //glBufferData(GL_ARRAY_BUFFER, 3 * sizeof(struct GLVertex), clientBuffer, GL_DYNAMIC_READ);
   //vertex boiler plate-- set up that GLVertex struct with gl
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(struct GLVertex), NULL); //x y z part
	glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(struct GLVertex), 3 * sizeof(float)); //r g b part
  glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, sizeof(struct GLVertex), 6 * sizeof(float)); //n.x n.y n.z part
	glEnableVertexAttribArray(0);
	glEnableVertexAttribArray(1);
  glEnableVertexAttribArray(2);
	//glClearColor(0,1,0,1); //for testing
	//glDisable(GL_CULL_FACE); //be sure to disable this later
	SDL_Event* even_t = calloc(sizeof(SDL_Event), 1);
	const Uint8 *state = SDL_GetKeyboardState(NULL);
	float offx = 0, offy = 0, offz = 0;
	/*while(1){ for manual debug
				glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
				//actual rendering happens here
				glDrawArrays(GL_TRIANGLE_STRIP, 0, coneSize * 3);
				//glDrawArrays(GL_TRIANGLE_STRIP, 1, coneSize * 3);
				//glDrawArrays(GL_TRIANGLE_STRIP, 2, coneSize * 3);
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
				if(state[SDL_SCANCODE_B]){
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
					vec3 tmp = {-15 + offx,-15 + offy,-45 + offz};
					printf("location:{%-+6.2f\t%-+6.2f\t%-+6.2f}\n",tmp[0],tmp[1],tmp[2]);
				    glm_translate_make(model,tmp);
				    glUniformMatrix4fv(locModel,1,GL_FALSE,model);
				}
	}*/
	//yes
	for(int anglex = 0; anglex < 36000; anglex += ITER_FACTOR){
	for(int angley = 0; angley < 36000; angley += ITER_FACTOR){
	for(int anglez = 0; anglez < 36000; anglez += ITER_FACTOR){
	for(int locationx = -4500; locationx < 4500; locationx += ITER_FACTOR){
	for(int locationy = -4500; locationy < 4500; locationy += ITER_FACTOR){
	for(int locationz = -4500; locationz < 4500; locationz += ITER_FACTOR){
	for(int colorr = 0; colorr < 100; colorr += ITER_FACTOR){
	for(int colorg = 0; colorg < 100; colorg += ITER_FACTOR){
	for(int colorb = 0; colorb < 100; colorb += ITER_FACTOR){
	for(int lightr = 0; lightr < 100; lightr += ITER_FACTOR){
	for(int lightg = 0; lightg < 100; lightg += ITER_FACTOR){
	for(int lightb = 0; lightb < 100; lightb += ITER_FACTOR)
	{//hi i'm bob
			zenith[0] = locationx / 100.0; zenith[1] = locationy / 100.0; zenith[2] = locationz / 100.0; //can't be bothered
			vec3 tmp = {locationx / 100.0,locationy / 100.0,locationz / 100.0};

			glm_look(zero, zenith, up, view);
			glUniformMatrix4fv(locView,1,GL_FALSE,view);
			glm_translate_make(model,tmp);
			glUniformMatrix4fv(locModel,1,GL_FALSE,model);

			glUniform3f(locOver,colorr / 100.0,colorg / 100.0,colorb / 100.0);

			UpdateLight((vec3){-15,-5,-45},(vec3){lightr / 100.0,lightg / 100.0,lightb / 100.0},program);

			//render
			glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
			glDrawArrays(GL_TRIANGLE_STRIP, 0, coneSize * 3);
			SDL_GL_SwapWindow(window);
			//so you're telling me its all negative fourty-five?
			//always has been
			ScreenShot((vec3){anglex / 100.0,angley / 100.0,anglez / 100.0},(vec3){locationx / 100.0,locationy / 100.0,locationz / 100.0},(vec3){colorr / 100.0,colorg / 100.0,colorb / 100.0});

	}}}}}}}}}}}}
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
//yes C# has rubbed off on me... yes i know its bad
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
void ScreenShot(vec3 angle, vec3 location, vec3 color){
    int depth = 8 + 8 + 8 + 8, /*8 red bits, 8 green bits, 8 blue bits, 8 alpha bits*/ pitch = (depth / 8) * WINDOW_WIDTH; //pitch is in bytes so div depth by 8
    glCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 0, 0, WINDOW_WIDTH, WINDOW_HEIGHT);
    glGetTexImage(GL_TEXTURE_2D, 0, GL_BGRA, GL_UNSIGNED_BYTE, pixels);
    char name[512];
	memcpy(name, PATH_TO_OUT, 16); //i love magic!
	sprintf(name + 16, "%d.png\0", current);
	printf("name1=%s",name);
    if(IMG_SavePNG(flip_surface(SDL_CreateRGBSurfaceFrom(pixels, WINDOW_WIDTH, WINDOW_HEIGHT, depth, pitch, 0, 0, 0, 0)), name))
				printf("Failed to save PNG for iteration %s\n", current);
	//write meta file
	memcpy(name, PATH_TO_META, 12);
	sprintf(name + 12, "%d.txt\0", current);
	printf("name2=%s\n",name);
	FILE* fp = fopen(name, "w");
	fprintf(fp,"%f\n%f\n%f\n%f\n%f\n%f\n%f\n%f\n%f\n",angle[0],angle[1],angle[2],location[0],location[1],location[2],color[0],color[1],color[2]);
	fclose(fp);
	//incremeant current for next iteration
	current++;
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
//use this https://github.com/admesh/admesh/blob/master/src/stl_io.c
//okay done (yes i was talking to myself)
unsigned int* ReadSTLFile(const char* filename){
	stl_file fp;
	stl_open(&fp, "cone.stl");
	//stl_verify_neighbors(%fp);
	//stl_fill_holes(&fp);
	//do ops here
	long size = fp.stats.number_of_facets;
	printf("size: %d\n",size);
	struct GLVertex* verts = calloc(sizeof(struct GLVertex), size * 3 + 3);
	unsigned int* indices = calloc(sizeof(int),size * 3 + 3);
	for(int i = 0; i < size; ++i){
		stl_facet* cur = fp.facet_start + i;
		for(int j = 0; j < 3; ++j){
			//indices[i + j] = fp.v_indices[i].vertex[j];
			verts[i + j].x = cur->vertex[j].x / SCALE_DIV_FACTOR;
			verts[i + j].y = cur->vertex[j].y / SCALE_DIV_FACTOR;
			verts[i + j].z = cur->vertex[j].z / SCALE_DIV_FACTOR;
			verts[i + j].nx = cur->normal.x;
			verts[i + j].ny = cur->normal.y;
			verts[i + j].nz = cur->normal.z;
			verts[i + j].r = 1;
		}
	}
	stl_close(&fp);
	//welcome to C
	unsigned int* a = calloc(sizeof(int),3);
	a[0] = verts;
	a[1] = size;
	a[2] = indices;
	return a;
}
