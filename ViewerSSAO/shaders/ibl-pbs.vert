#version 330

layout (location = 0) in vec3 vert;
layout (location = 1) in vec3 normal;
layout (location = 2) in vec2 texCoord;

uniform mat4 projection;
uniform mat4 view;
uniform mat4 model;

uniform mat3 normal_matrix;
uniform vec3 light;

out vec3 world_normal;
out vec3 world_pos;
out vec3 view_vec;
out vec3 light_vec;
out vec3 normal_vec;
out vec2 vTexCoords;

void main(void)  {
    world_normal = mat3(transpose(inverse(model)))*normal;
    world_pos = vec3(model*vec4(vert,1.0));
    normal_vec = normal_matrix * normal;
    view_vec = -(view*model*vec4(vert,1.0)).xyz;
    light_vec = (view*vec4(light,1.0)).xyz + view_vec;
    //light_vec = view_vec;
    vTexCoords = texCoord;
    gl_Position = projection * view * model* vec4(vert,1.0);
}
