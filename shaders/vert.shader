#version 330

layout(location = 0)in vec3 pos;
layout(location = 1)in vec3 clr;
layout(location = 2)in vec3 normal;

out vec3 outClr;
out vec3 nnormal;
out vec3 worldspace;

uniform mat4 projection;
uniform mat4 view;
uniform mat4 model;

void main()
{
    gl_Position = projection * view * model * vec4(pos,1.0);
		outClr = clr;
		worldspace = (model * vec4(pos,1.0)).xyz;
		nnormal = normal;
}