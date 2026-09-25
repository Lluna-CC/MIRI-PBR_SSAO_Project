#version 330

out vec4 frag_color;
uniform sampler2D current_texture;
uniform float near;
uniform float far;

in vec2 vTexCoords;

void main (void) {
    frag_color = texture(current_texture, vTexCoords);
    if (near >= 0) {
        float frag_depth = texture(current_texture, vTexCoords).r;
        float z_ndc = 2.0*frag_depth - 1.0;
        float z_eye = 2.0 * near * far /(far + near - z_ndc * (far - near));
        
        frag_color.rgb = 1 - vec3(z_eye/(far - near));
    }
    
}
