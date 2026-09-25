#version 330
//Change this value to change the reflection
//This can be used to test a part of the IBL shader
float roughness_lod = 0.0;

layout (location = 0) out vec4 frag_color;
layout (location = 1) out vec3 normals;

in vec3 world_normal;
in vec3 eye_normal;
in vec3 world_pos;

uniform samplerCube specular_map;
uniform samplerCube prefilter_map;
uniform mat4 view;

void main (void) {
    vec3 cameraPos = vec3(inverse(view) * vec4(0.0,0.0,0.0,1.0));

    vec3 incident = normalize(world_pos-cameraPos);
    vec3 reflected = reflect(incident,normalize(world_normal));

    frag_color = textureLod(prefilter_map, reflected,roughness_lod);
    normals = normalize(eye_normal);
    //frag_color = vec4(1.0,0.0,1.0,1.0);
}
