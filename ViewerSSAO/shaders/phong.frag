#version 330
//#define MAPS
#ifdef MAPS
uniform sampler2D color_map;
#else
uniform vec4 matAmbient = vec4(0.97, 0.51, 0.47,1.0);
uniform vec4 matDiffuse = vec4(0.97, 0.51, 0.47,1.0);

#endif


layout (location = 1) out vec3 normals;
layout (location = 0) out vec4 frag_color;

in vec2 vTexCoords;

in vec3 view_vec;
in vec3 light_vec;
in vec3 normal_vec;

uniform vec4 lightAmbient = vec4(0.3,0.3,0.3,1.0);
uniform vec4 lightDiffuse = vec4(1.0,1.0,1.0,1.0);
uniform vec4 lightSpecular = vec4(1.0,1.0,1.0,1.0);


uniform vec4 matSpecular = vec4(0.97, 0.51, 0.47,1.0);
uniform float shininess = 10; 

vec4 phong(vec3 N, vec3 V, vec3 L) {
    N = normalize(N); V = normalize(V); L = normalize(L);
    vec3 R = reflect(-L,N);
    float NdotL = max(0.0, dot(N,L));
    float RdotV = max(0.0, dot(R,V));

    float Idiff = NdotL;
    float Ispec = 0;
    if (NdotL > 0) Ispec = pow(RdotV, shininess);
    
    
    #ifdef MAPS
    vec4 albedoAmbient = texture(color_map, vTexCoords);
    vec4 albedoDiffuse = albedoAmbient;
    #else
    vec4 albedoAmbient = matAmbient;
    vec4 albedoDiffuse = matDiffuse;
    #endif

    vec4 res =  albedoAmbient * lightAmbient + albedoDiffuse * lightDiffuse * Idiff + matSpecular * lightSpecular * Ispec;
    return res;
}

void main (void) {
    frag_color = phong(normal_vec, view_vec, light_vec);
    normals = normalize(normal_vec);
}

