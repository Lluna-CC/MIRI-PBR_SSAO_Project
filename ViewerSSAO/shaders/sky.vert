#version 330

layout (location = 0) in vec3 vert;
layout (location = 1) in vec2 texCoord;

out vec3 TexCoords;

uniform mat4 projection;
uniform mat4 view;
uniform mat4 model;

void main(void)  {
    TexCoords = vert;
    vec4 temp = projection*view*vec4(vert,1.0);
    gl_Position = temp;
}

