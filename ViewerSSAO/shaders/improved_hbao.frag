#version 330
#define PI 3.1415926538

out vec4 frag_color;
uniform sampler2D normals;
uniform sampler2D depth;
uniform sampler2D albedo;
uniform sampler2D noise;
in vec2 vTexCoords;


uniform float radius;
uniform int nDirections;
uniform int nSamples;
uniform float near;
uniform float far;
uniform float fov;
uniform float aspect_ratio;

float height;
float width;
vec2 noiseScale;

vec3 point_on_sphere(float u, float v, float r) {
    float x = r*sin(u)*cos(v);
    float y = r*sin(u)*sin(v);
    float z = r*cos(u);
    return vec3(x,y,z);
}

vec3 transformToEyeSpace(vec2 screenPos) {
    vec2 pos_ndc = 2.0*screenPos - vec2(1.0,1.0);
    float frag_depth = texture(depth, screenPos).r;
    float z_ndc = 2.0*frag_depth - 1.0;
    float z_eye = 2.0 * near * far /(far + near - z_ndc * (far - near));
    
    float x_near = pos_ndc.x * width;
    float y_near = pos_ndc.y * height;
    
    float x_eye = x_near * z_eye / near;
    float y_eye = y_near * z_eye / near; 

    return vec3(x_eye,y_eye,z_eye);
    
}


vec2 transformToScreenSpace(vec3 position) {
    float x_eye = position.x;
    float y_eye = position.y;
    float z_eye = position.z;

    float x_near = x_eye * near /  z_eye;
    float y_near = y_eye * near / z_eye;

    float x_ndc = x_near / width;
    float y_ndc = y_near / height;
    vec2 uvcoords = (vec2(x_ndc,y_ndc) + 1)/2.0;
    return uvcoords;
}

float kernel(vec3 position) {
    vec3 normal = normalize(texture(normals, vTexCoords).xyz);
    vec3 randomVec = texture(noise, vTexCoords).xyz;

    vec3 tangent = normalize(randomVec - normal*dot(randomVec, normal));
    vec3 bitangent = cross(normal,tangent);
    
    float AO;

    for (int i = 0; i <= nDirections; ++i) {
        float theta = -PI + i*2*PI/(nDirections) ;
        
        vec3 dist = normalize(vec3(cos(theta), sin(theta), position.z));
        vec3 T = normalize(dist - normal);
        float t = atan(T.z/length(T.xy));

        float horizon = 0.0;
        float horizonDist = 0.0;
        for (int j = 0; j < nSamples; ++j) {
            float act_r = (j + 1.0)*radius/nSamples;
            vec3 sample = vec3(cos(theta)*act_r,sin(theta)*act_r, position.z);
            vec3 eye_sample = sample.x * bitangent + sample.y * tangent + sample.z * normal;
        
            vec2 sample_uv = transformToScreenSpace(eye_sample);
            if (sample_uv.x > 1.0 || sample_uv.y > 1.0 || sample_uv.x < 0.0 || sample_uv.x < 0.0) continue;
            sample = transformToEyeSpace(sample_uv);

            vec3 D = sample - position;
            if (length(D) <= radius) {
                float elevation = atan(-D.z/length(D.xy));
                if (elevation > horizon) {
                    horizon = elevation;
                    horizonDist = length(D);
                } 
            }
        }
        AO += (max(0,1 - horizonDist/radius))* (sin(horizon) - sin(t));
    }

    return 1 - AO/(2*PI*nDirections);
}

float kernel(vec3 position, float r) {
    vec3 normal = texture(normals, vTexCoords).xyz;
    normal = normalize(normal); //It is supposed to be already normlized, but to be sure
    vec2 randomVec = normalize(texture(noise, vTexCoords*noiseScale).xy);
 

    float AO = 0.0;

    for (int i = 1; i <= nDirections; ++i) {
        float theta = -PI + i*2*PI/(nDirections);
        vec2 imageDir = vec2(cos(theta),sin(theta)) + randomVec; //Normalized direction in image space
        imageDir = normalize(imageDir);
        vec3 eyeDir = vec3(imageDir,0.0); //In eye space we have the same direction
        vec3 tangent = normalize(eyeDir - normal*dot(normal,eyeDir));
        float t = atan(tangent.z/length(tangent.xy));

        float horizon = t; //t is the minimum value for the horizon
        float horizonDist = 0.0;
        
        for (int j = 1; j <= nSamples; ++j) {
            float act_r = j*r/nSamples;
            vec2 imageSample = imageDir*act_r + vTexCoords;
            if (imageSample.x > 1.0 || imageSample.y > 1.0 || imageSample.x < 0.0 || imageSample.x < 0.0) continue;
            
            vec3 sample = transformToEyeSpace(imageSample); //sample in eye space
       
            vec3 D = sample - position;
            if (length(D) <= radius) {
                float elevation = atan(-D.z/length(D.xy));
                if (elevation > horizon) {
                    horizon = elevation;
                    horizonDist = length(D);
                } 
            }
        }
        float new_radius = max(radius, 0.000001);
        AO += (max(0,1 - horizonDist/new_radius))* (sin(horizon) - sin(t));
    }

    return 1 - AO/(nDirections);
}

void main (void) {
    //frag_color = texture(normals, vTexCoords);
    height = sin(fov)*near;
    width = height * aspect_ratio;
    noiseScale = vec2(width/32.0, height/32.0);

    vec3 position = transformToEyeSpace(vTexCoords);
    vec2 position_radius = transformToScreenSpace(position + vec3(radius,0.0,0.0));
    float screen_radius = abs(position_radius.x - vTexCoords.x);
    
    float ambient_occlusion = kernel(position, screen_radius);

    
    frag_color = texture(albedo,vTexCoords);
    frag_color.rgb = vec3(ambient_occlusion);
    frag_color.a = 1.0;
    //frag_color.r = screen_radius*2; 
}

