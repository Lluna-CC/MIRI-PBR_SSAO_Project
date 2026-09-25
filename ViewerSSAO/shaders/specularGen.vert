#version 330

layout (location = 0) in vec3 vert;

out vec3 vTexCoords;

uniform mat4 direction;


void main(void)  {
    mat4 flip = mat4(-1,0,0,0,0,-1, 0, 0,0, 0, 1, 0,0,0,0,1);
    vTexCoords = (direction*vec4(vert,1.0)).xyz;
    gl_Position = flip*vec4(vert,1.0);
}

