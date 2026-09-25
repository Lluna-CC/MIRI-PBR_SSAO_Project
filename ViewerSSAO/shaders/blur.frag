#version 330

#define PI 3.1415926538
out vec4 frag_color;
uniform sampler2D scene_text;
uniform sampler2D albedo;

in vec2 vTexCoords;



float gaussianKern(float std, float x) {
    return 1/(2*PI*std*std)*exp(-x*x/(2*std*std));
}
//range has to be an odd number
vec3 gaussianBlur(int range) {
    vec2 texelSize = 1.0 / vec2(textureSize(scene_text, 0));
    vec3 result = vec3(0.0);
    float weightSum = 0;
    int one_directon_range = (range - 1)/2;
    for (int x = -one_directon_range; x <= one_directon_range; ++x) {
         float u = float(x) * texelSize.x;
        if (u > 1 || u < 0) continue;
        for (int y = -one_directon_range; y <= one_directon_range; ++y) {
            float v = float(y) * texelSize.y;
            if (v > 1 || v < 0) continue;
            vec2 texel = vTexCoords + vec2(u,v);
            float weight = gaussianKern(1.0,length(vTexCoords - texel));
            result += (texture(scene_text, texel).rgb)*weight;
            weightSum += weight;
        }
    }
    return result/weightSum;
}

vec3 bilateralBlur(int range) {
    vec2 texelSize = 1.0 / vec2(textureSize(scene_text, 0));
    vec3 result = vec3(0.0);
    float weightSum = 0;
    int one_directon_range = (range - 1)/2;
    vec3 centerCoor = texture(scene_text, vTexCoords).rgb;
    for (int x = -one_directon_range; x <= one_directon_range; ++x) {
         float u = float(x) * texelSize.x;
        if (u > 1 || u < 0) continue;
        for (int y = -one_directon_range; y <= one_directon_range; ++y) {
            float v = float(y) * texelSize.y;
            if (v > 1 || v < 0) continue;
            vec2 texel = vTexCoords + vec2(u,v);
            vec3 tex_col = texture(scene_text, texel).rgb;
            float weight = gaussianKern(1.0,length(vTexCoords - texel)) * gaussianKern(1.0,length(centerCoor - tex_col));
            result += tex_col*weight;
            weightSum += weight;
        }
    }
    return result/weightSum;
}
void main (void) {

    vec3 result = bilateralBlur(3);
    frag_color = texture(albedo,vTexCoords);
    frag_color.rgb *= result;
}
