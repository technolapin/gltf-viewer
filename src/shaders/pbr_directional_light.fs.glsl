#version 330

in vec3 vViewSpaceNormal;
in vec2 vTexCoords;
in vec3 vViewSpacePosition;

uniform vec3 uLightDir;
uniform vec3 uLightCol;

uniform sampler2D uBaseColorTexture;

uniform vec4 uBaseColorFactor;

uniform float uMetallicFactor;
uniform float uRoughnessFactor;
uniform sampler2D uMetallicRoughnessTexture;

uniform vec3 uEmissiveFactor;
uniform sampler2D uEmissiveTexture;

uniform float uOcclusionFactor;
uniform sampler2D uOcclusionTexture;

out vec3 fColor;

// Constants
const float GAMMA = 2.2;
const float INV_GAMMA = 1. / GAMMA;
const float M_PI = 3.141592653589793;
const float M_1_PI = 1.0 / M_PI;

// We need some simple tone mapping functions
// Basic gamma = 2.2 implementation
// Stolen here:
// https://github.com/KhronosGroup/glTF-Sample-Viewer/blob/master/src/shaders/tonemapping.glsl

// linear to sRGB approximation
// see http://chilliant.blogspot.com/2012/08/srgb-approximations-for-hlsl.html
vec3 LINEARtoSRGB(vec3 color) { return pow(color, vec3(INV_GAMMA)); }

// sRGB to linear approximation
// see http://chilliant.blogspot.com/2012/08/srgb-approximations-for-hlsl.html
vec4 SRGBtoLINEAR(vec4 srgbIn)
{
  return vec4(pow(srgbIn.xyz, vec3(GAMMA)), srgbIn.w);
}

float pow5(float a)
{
    float aa = a*a;
    float aaaa = aa*aa;
    return aaaa*a;
}


float fresnel_mix(float ior, float base, float layer, float VdotH)
{
    float f0sqrt = (1-ior)/(1+ior);
    float f0 = f0sqrt * f0sqrt;
    float fr = f0 + (1- f0) * pow5(1 - abs(VdotH));
    return mix(base, layer, fr);
}


void main()
{
  vec3 N = normalize(vViewSpaceNormal);
  vec3 L = uLightDir;
  vec3 V = normalize(-vViewSpacePosition);
  vec3 H = normalize(L+V);
  
  vec4 metallicFactors = texture(uMetallicRoughnessTexture, vTexCoords);

  float baseRoughness = metallicFactors.g;
  float baseMetallic = metallicFactors.b;
  
  
  vec4 baseColorFromTexture = 
      SRGBtoLINEAR(texture(uBaseColorTexture, vTexCoords));
  vec4 baseColor = baseColorFromTexture * uBaseColorFactor;


  float roughness = baseRoughness*uRoughnessFactor;
  float metallic = baseMetallic*uMetallicFactor;
  
  float alpha = roughness*roughness;
  float alpha2 = alpha*alpha;

  float NdotH = clamp(dot(N, H), 0., 1.);
  float NdotH2 = NdotH*NdotH;
  float NdotL = clamp(dot(N, L), 0., 1.);
  float NdotL2 = NdotL*NdotL;
  float NdotV = clamp(dot(N, V), 0., 1.);
  float NdotV2 = NdotV*NdotV;
  float HdotL = clamp(dot(H, L), 0., 1.);
  float HdotV = clamp(dot(H, V), 0., 1.);

  float div_D = (NdotH2 * (alpha2 - 1) + 1);
  float div_D2 = div_D * div_D;
  
  float D = alpha2 * NdotH / (M_PI * div_D2);

  float V_divL = NdotL + sqrt(alpha2 + (1 - alpha2)*NdotL2);
  float V_divV = NdotV + sqrt(alpha2 + (1 - alpha2)*NdotV2);
  float V_div = V_divL * V_divV;

  float Vis = HdotL * HdotV / V_div;

  if (V_div <= 0)
  {
      Vis = 0;
  }
  

  float dielectricSpecular = 0.04;
  
  vec3 black = vec3(0);
      
  vec3 c_diff = mix(baseColor.rgb * (1.0 - dielectricSpecular), black, metallic); 
  vec3 f0 = mix(vec3(dielectricSpecular), baseColor.rgb, metallic);
  vec3 F = f0 + (vec3(1.0) - f0) * pow5(1.0 - abs(HdotV));

  
  vec3 f_diffuse = (vec3(1.0) - F) * c_diff * M_1_PI;
  vec3 f_specular = F * D * Vis;

  vec3 material = f_diffuse + f_specular;

  vec3 emission = texture(uEmissiveTexture, vTexCoords).rgb*uEmissiveFactor;
  
  vec3 color = (f_diffuse + f_specular) * uLightCol * NdotL + emission;

  if (uOcclusionFactor > 0)
  {
      float occl = texture2D(uOcclusionTexture, vTexCoords).r;
      color = mix(color, color * occl, uOcclusionFactor);
  }
  
  fColor = LINEARtoSRGB(color);
}
