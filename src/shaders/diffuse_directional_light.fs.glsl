#version 330

in vec3 vViewSpacePosition;
in vec3 vViewSpaceNormal;
in vec2 vTexCoords;

out vec3 fColor;

uniform vec3 uLightDir;
uniform vec3 uLightCol;

const float PI = 3.14;

vec3
brdf(vec3 wi, vec3 wo)
{
    return vec3(1.0/PI);
}

vec3
radiance(vec3 light_dir, vec3 view_dir, vec3 light_color, vec3 normal)
{
    // cos(angle(normal, light_dir)) =  - light_dir · normal (car normalisés)
    float cos_alpha = dot(light_dir, normal);
    return brdf(light_dir, view_dir)*light_color*cos_alpha;
}



void main()
{
    vec3 viewDirection = normalize(-vViewSpacePosition);
    // Need another normalization because interpolation of vertex attributes does not maintain unit length
    vec3 viewSpaceNormal = normalize(vViewSpaceNormal);
    
    fColor = radiance(uLightDir, viewDirection, uLightCol, viewSpaceNormal);
}
