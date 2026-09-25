#version 330 core
layout (location = 1) out vec3 normals;
layout (location = 0) out vec4 albedo;

in vec2 vTexCoords;
in vec3 normal_vec;

void main()
{
    // also store the per-fragment normals into the gbuffer
    normals = normalize(normal_vec);
    // and the diffuse per-fragment color, ignore specular
    albedo = vec4(1.0,1.0,1.0,1.0);
}