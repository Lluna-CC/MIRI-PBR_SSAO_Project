#version 330

layout (location = 0) in vec3 vert;
layout (location = 1) in vec3 normal;
layout (location = 2) in vec2 texCoord;

uniform mat4 projection;
uniform mat4 view;
uniform mat4 model;

uniform mat3 normal_matrix;

out vec3 world_normal;
out vec3 world_pos;
out vec3 eye_normal;

void main(void)  {
    eye_normal = normal_matrix*normal;
    world_normal = mat3(transpose(inverse(model)))*normal;
    world_pos = vec3(model*vec4(vert,1.0));
    gl_Position = projection * view * model* vec4(vert,1.0);
}
