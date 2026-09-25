#version 330
#define PI 3.1415926538

out vec4 frag_color;
uniform samplerCube specular_map;
in vec3 vTexCoords;

//This function could be used to add randomness to the sampling, but I have not tested it in depth
float hash11( uint n ) 
{
    // integer hash copied from Hugo Elias
	n = (n << 13U) ^ n;
    n = n * (n * n * 15731U + 789221U) + 1376312589U;
    return float( n & uint(0x7fffffffU))/float(0x7fffffff);
}


vec3 point_on_sphere(float u, float v, float r) {
    float x = r*sin(u)*cos(v);
    float y = r*sin(u)*sin(v);
    float z = r*cos(u);
    return vec3(x,y,z);
}

void main (void) {
    //We can take the vertex coordinates as the normal vector
    vec3 norm = normalize(vTexCoords);

    vec3 irradiance = vec3(0.0);
    vec3 up_vec = vec3(0.0,1.0,0.0);
    vec3 right_vec = normalize(cross(up_vec, norm));
    up_vec = normalize(cross(norm,right_vec)); 

    float sampleDelta = 0.025;
    int nSamples = 0;
    for (float phi = 0.0; phi < 2.0* PI; phi += sampleDelta)
        for (float theta = 0.0; theta < 0.5*PI; theta += sampleDelta) {
            vec3 sample = point_on_sphere(theta,phi, 1);

            vec3 world_sample = sample.x * right_vec + sample.y * up_vec + sample.z*norm;

            irradiance += texture(specular_map, world_sample).rgb*cos(theta)*sin(theta);
            ++nSamples;
        }

    irradiance = (PI * irradiance)/(float(nSamples));
    frag_color = vec4(irradiance,1.0);
}
