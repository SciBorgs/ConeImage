#version 330

layout(location = 0)in vec3 pos;
layout(location = 1)in vec3 clr;

out vec3 outClr;

//uniform mat4 projection;
//uniform mat4 view;
//uniform mat4 model;

//model * view * projection * pos <- do when done

void main()
{
    gl_Position = vec4(pos,1.0);
		outClr = clr;
}