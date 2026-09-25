#version 330

layout (location = 0) in vec3 vert;
layout (location = 1) in vec3 normal;
layout (location = 2) in vec2 texCoord;

out vec2 vTexCoords;

uniform mat4 projection;
uniform mat4 view;
uniform mat4 model;

uniform mat3 normal_matrix;

out vec3 view_vec;
//out vec3 light_vec;
out vec3 normal_vec;

uniform vec3 light;

void main(void)  {
    vTexCoords = texCoord;
    normal_vec = normal_matrix * normal;
    //view_vec = -(view*model*vec4(vert,1.0)).xyz;
    //light_vec = (view*vec4(light,1.0)).xyz + view_vec;
    
    gl_Position = projection*view*model*vec4(vert,1.0);
}
