#version 330

#define PI 3.1415926538
#define MAPS
float gamma = 2.2;
int D_function = 2;
int G_function = 0;


layout (location = 0) out vec4 frag_color;
layout (location = 1) out vec3 normals;
 
in vec2 vTexCoords;

in vec3 view_vec;
in vec3 light_vec;
in vec3 normal_vec;


uniform vec3 fresnel;

#ifdef MAPS
uniform sampler2D roughness_map;
uniform sampler2D metalness_map;
uniform sampler2D color_map;
#else

uniform float metalness;
uniform float roughness;
#endif

/*vec4 phong(vec3 N, vec3 V, vec3 L) {
    N = normalize(N); V = normalize(V); L = normalize(L);
    vec3 R = reflect(-L,N);
    float NdotL = max(0.0, dot(N,L));
    float RdotV = max(0.0, dot(R,V));

    float Idiff = NdotL;
    float Ispec = 0;
    if (NdotL > 0) Ispec = pow(RdotV, shininess);
    return matAmbient * lightAmbient + matDiffuse * lightDiffuse * Idiff + matSpecular * lightSpecular * Ispec;

}*/

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


float TrowbridgeReitzNormalDistribution(float NdotH, float roughness){
    float roughnessSqr = roughness*roughness;
    float Distribution = NdotH*NdotH * (roughnessSqr-1.0) + 1.0;
    return roughnessSqr / (3.1415926535 * Distribution*Distribution);
}


float GGXNormalDistribution(float roughness, float NdotH)
{
    float roughnessSqr = roughness*roughness;
    float NdotHSqr = NdotH*NdotH;
    float TanNdotHSqr = (1-NdotHSqr)/NdotHSqr;
    return (1.0/3.1415926535) * sqrt(roughness/(NdotHSqr * (roughnessSqr + TanNdotHSqr)));
}


float GaussianNormalDistribution(float roughness, float NdotH)
{
    float roughnessSqr = roughness*roughness;
	float thetaH = acos(NdotH);
    return exp(-thetaH*thetaH/roughnessSqr);
}


float BeckmannNormalDistribution(float roughness, float NdotH)
{
    float roughnessSqr = roughness*roughness;
    float NdotHSqr = NdotH*NdotH;
    return max(0.000001,(1.0 / (3.1415926535*roughnessSqr*NdotHSqr*NdotHSqr))
* exp((NdotHSqr-1)/(roughnessSqr*NdotHSqr)));
}

vec3 Flambert(vec3 albedo) {
    return (albedo/PI);
}


float MicroGeometryDistribution(vec3 h, vec3 n, float rough) {
    float ndoth = max(0.00000001, dot(h,n));
    if (D_function == 0) return max(0.000001, BeckmannNormalDistribution(rough, ndoth));
    else if (D_function == 1) return max(0.000001, TrowbridgeReitzNormalDistribution(rough, ndoth));
    else if (D_function == 2) return max(0.000001, GGXNormalDistribution(rough, ndoth));
    return 0.f;
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
    float ldoth = max(0.000000, dot(l,h));
    return F0 + (1 -  F0) *  pow((1 - (ldoth)),5);
}


vec3 pbs(vec3 l, vec3 v, vec3 n, float met, float rough, vec3 albedo) {
    n = normalize(n); v = normalize(v); l = normalize(l);
    vec3 h = normalize(l + v);
    vec3 F0 = mix(fresnel, albedo, met);

    float ndotl = max(0.0000001, dot(n,l));
    float ndotv = max(0.0000001, dot(n,v));
    float ldotv = max(0.000000, dot(l,v));
    

    vec3 F = FresnelReflectanceFunction(F0,l,h);
    float G = GeometryFunction(l,v,h,n);
    float D = MicroGeometryDistribution(h,n, rough);
    vec3 fs =  (F*G*D)/(4 * ndotl * ndotv);
   
    vec3 fd = Flambert(albedo);
    
    vec3 ks = F;
    vec3 kd = vec3(1) - ks;
    kd *= 1 - met; 

    vec3 res = kd*fd + fs; 
    res *= ndotl;
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
    vec3 light = pbs(light_vec,view_vec,normal_vec, met, rough, albedo);
    frag_color = vec4(light,1.0);
    frag_color.rgb = pow(frag_color.rgb, vec3(1.0/gamma));
    normals = normalize(normal_vec);
}

