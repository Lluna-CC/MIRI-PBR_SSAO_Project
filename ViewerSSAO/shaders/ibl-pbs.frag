#version 330

//Comment or uncoment this line to select whether the color, roughness and metalness is given by textures or uniforms
#define MAPS

//Change the values of theese parameters to change the gamma and the functions used for the computation
float gamma = 2.2;
int G_function = 0;


layout (location = 0) out vec4 frag_color;
layout (location = 1) out vec3 normals;

in vec3 world_normal;
in vec3 world_pos;
in vec3 view_vec;
in vec3 light_vec;
in vec3 normal_vec;
in vec2 vTexCoords;

uniform mat4 view;
uniform samplerCube diffuse_map;
uniform samplerCube specular_map;
uniform samplerCube prefilter_map;
uniform vec3 fresnel;
#ifdef MAPS
uniform sampler2D roughness_map;
uniform sampler2D metalness_map;
uniform sampler2D color_map;
#else

uniform float metalness;
uniform float roughness;
#endif

float ImplicitGeometricShadowingFunction (float NdotL, float NdotV){
	float Gs =  (NdotL*NdotV);       
	return Gs;
}


float AshikhminShirleyGSF (float NdotL, float NdotV, float LdotH){
	float Gs = NdotL*NdotV/(LdotH*max(NdotL,NdotV));
	return  (Gs);
}


float AshikhminPremozeGeometricShadowingFunction (float NdotL, float NdotV){
	float Gs = NdotL*NdotV/(NdotL+NdotV - NdotL*NdotV);
	return  (Gs);
}


float NeumannGeometricShadowingFunction (float NdotL, float NdotV){
	float Gs = (NdotL*NdotV)/max(NdotL, NdotV);       
	return  (Gs);
}


float KelemenGeometricShadowingFunction (float NdotL, float NdotV, 
float LdotV, float VdotH){
	float Gs = (NdotL*NdotV)/(VdotH * VdotH); 
	return   (Gs);
}


float ModifiedKelemenGeometricShadowingFunction (float NdotV, float NdotL,
 float roughness)
{
	float c = 0.797884560802865;    // c = sqrt(2 / Pi)
	float k = roughness * roughness * c;
	float gH = NdotV  * k +(1-k);
	return (gH * gH * NdotL);
}


float CookTorrenceGeometricShadowingFunction (float NdotL, float NdotV, 
float VdotH, float NdotH){
	float Gs = min(1.0, min(2*NdotH*NdotV / VdotH, 
    2*NdotH*NdotL / VdotH));
	return  (Gs);
}


float GeometryFunction(vec3 l, vec3 v, vec3 h,vec3 n) {
    float ldoth = max(0.0000000, dot(l,h));
    float ndotl = max(0.0000000, dot(n,l));
    float ndotv = max(0.0000000, dot(n,v));
    float vdoth = max(0.0000000, dot(v,h));
    float ndoth = max(0.0000000, dot(n,h));
    if (G_function == 0) return CookTorrenceGeometricShadowingFunction(ndotl, ndotv,vdoth, ndoth);
    else if (G_function == 1) return AshikhminPremozeGeometricShadowingFunction(ndotl, ndotv);
    else if (G_function == 2) return NeumannGeometricShadowingFunction(ndotl,ndotv);
    return 0.f;
}

vec3 FresnelReflectanceFunction(vec3 F0, vec3 l, vec3 h) {
    float ldoth = max(0.0, dot(l,h));
    return F0 + (1 -  F0) *  pow((1 - (ldoth)),5);
}

vec3 ibl_pbs(vec3 l, vec3 v, vec3 n, float met, float rough, vec3 albedo) {
    n = normalize(n); v = normalize(v); l = normalize(l);
    
    //The halfway vector does not have much sense in this situtation (with ambient ligh). Still we are going to consider that light comes from the normal direction.
    vec3 h = normalize(l + n);
    //vec3 h = n;
    //vec3 h = normalize(l + v);
    
    vec3 F0 = mix(fresnel, albedo, met);

    float ndotl = max(0.0000001, dot(n,l));
    float ndotv = max(0.0000001, dot(n,v));
    float ldotv = max(0.0000001, dot(l,v));
    
    //Learn OpenGL suggests using the angle between l and n to compute the F_Schlick function
    vec3 F = FresnelReflectanceFunction(F0,l,n);


    float G = GeometryFunction(l,v,h,n);
    //float D = MicroGeometryDistribution(h,n, rough);
    
    vec3 cameraPos = vec3(inverse(view) * vec4(0.0,0.0,0.0,1.0));
    vec3 incident = normalize(world_pos-cameraPos);
    vec3 reflected = reflect(incident,normalize(world_normal));

    float roughness_lod = rough*5.0;
    vec3 D = textureLod(prefilter_map,reflected,roughness_lod).rgb;
    vec3 fs =  (F*G*D)/(4 * ndotv * ndotl);
   
    vec3 diff_irradiance = texture(diffuse_map, normalize(world_normal)).rgb;
    vec3 diffuse = albedo * diff_irradiance;
    
    vec3 ks = F;
    vec3 kd = vec3(1) - ks;
    kd *= 1 - met; 

    vec3 res = kd*diffuse + fs*ndotl; 
    //res *= ndotl;
    //res *= diffuse;
    return res;

}

void main (void) {
#ifdef MAPS
    float met = texture(metalness_map,vTexCoords).x;
    float rough = texture(roughness_map,vTexCoords).x;
    vec3 albedo = texture(color_map, vTexCoords).rbg;
#else
    float met = metalness;
    float rough = roughness;
    vec3 albedo = vec3(0.97, 0.51, 0.47);
#endif

    frag_color = vec4(ibl_pbs(view_vec,view_vec,normal_vec, met, rough, albedo),1.0);
    frag_color.rgb = pow(frag_color.rgb, vec3(1.0/gamma));
    normals = normalize(normal_vec);
}