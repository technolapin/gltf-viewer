#version 330

layout(location = 0) in vec3 aPosition;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec2 aTexCoords;
layout(location = 3) in vec3 aTangent;

out vec3 vWorldSpacePosition;
out vec3 vViewSpacePosition;
out vec3 vViewSpaceNormal;
out vec2 vTexCoords;
out mat3 vTBN;

uniform mat4 uModelViewProjMatrix;
uniform mat4 uModelViewMatrix;
uniform mat4 uModelMatrix;
uniform mat4 uNormalMatrix;

void main()
{
    vec3 T = normalize(vec3(uModelViewMatrix * vec4(aTangent,   0.0)));
    vec3 N = normalize(vec3(uModelViewMatrix * vec4(aNormal,    0.0)));
    vec3 B = cross(N, T);

    vTBN = mat3(T, B, N);
    
    vViewSpacePosition = vec3(uModelViewMatrix * vec4(aPosition, 1));
    vWorldSpacePosition = vec3(uModelMatrix * vec4(aPosition, 1));
    vViewSpaceNormal = normalize(vec3(uNormalMatrix * vec4(aNormal, 0)));
	vTexCoords = aTexCoords;
    gl_Position =  uModelViewProjMatrix * vec4(aPosition, 1);
}
