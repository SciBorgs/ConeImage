#version 330

in vec3 outClr;
out vec4 fragColor;

void main()
{
    fragColor = vec4(outClr, 1.0);
}