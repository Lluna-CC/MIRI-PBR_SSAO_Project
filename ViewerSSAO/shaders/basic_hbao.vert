#version 330

layout (location = 0) in vec3 vert;

out vec2 vTexCoords;
void main(void)  {
    vTexCoords = (vert.xy + vec2(1,1))*1/2;
    gl_Position = vec4(vert,1.0);
}
