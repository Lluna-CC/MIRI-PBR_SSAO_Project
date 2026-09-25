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
    vec3 randomVec = texture(noise, vTexCoords*noiseScale).xyz;

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

void main (void) {
    //frag_color = texture(normals, vTexCoords);
    height = sin(fov)*near;
    width = height * aspect_ratio;
    noiseScale = vec2(width/32.0, height/32.0);

    vec3 position = transformToEyeSpace(vTexCoords);
    vec2 position_radius = transformToScreenSpace(position + vec3(radius,0.0,0.0));
    float screen_radius = abs(position_radius.x - vTexCoords.x);
    
    float ambient_occlusion = kernel(position);

    
    frag_color = texture(albedo,vTexCoords);
    frag_color.rgb *= vec3(ambient_occlusion);
    frag_color.a = 1.0;
    //frag_color.r = screen_radius*2; 
}

