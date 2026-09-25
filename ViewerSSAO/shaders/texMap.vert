#version 330

layout (location = 0) in vec3 vert;
layout (location = 1) in vec3 normal;
layout (location = 2) in vec2 texCoord;


uniform mat4 projection;
uniform mat4 view;
uniform mat4 model;

out vec2 vTexCoords;


void main(void)  {
    vTexCoords = texCoord;
    gl_Position = projection*view*model*vec4(vert,1.0);
}
