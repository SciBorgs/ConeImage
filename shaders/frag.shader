#version 330

in vec3 outClr;
out vec4 fragColor;

float ambient = 0.1;
in vec3 nnormal;
in vec3 worldspace;

uniform vec3 lightPos;  
uniform vec3 lightColor;

void main()
{
    vec3 lightDir = normalize(lightPos - worldspace); //dir to light
		vec3 trueNormal = normalize(nnormal); //normalized face surface normal
    fragColor = vec4((max(dot(trueNormal, lightDir), 0.0) * lightColor) + ambient, 1.0) * vec4(outClr, 1.0);
}