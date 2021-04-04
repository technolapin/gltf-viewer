#version 330

in vec3 vViewSpaceNormal;
in vec2 vTexCoords;
in vec3 vViewSpacePosition;
in vec3 vWorldSpacePosition;

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

uniform sampler2D uNormalTexture;
uniform int uUseNormal;

uniform int uRenderMode;

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


mat3 inverse3x3( mat3 M )
{
    // The original was written in HLSL, but this is GLSL,
    // therefore
    // - the array index selects columns, so M_t[0] is the
    // first row of M, etc.
    // - the mat3 constructor assembles columns, so
    // cross( M_t[1], M_t[2] ) becomes the first column
    // of the adjugate, etc.
    // - for the determinant, it does not matter whether it is
    // computed with M or with M_t; but using M_t makes it
    // easier to follow the derivation in the text
    mat3 M_t = transpose( M );
    float det = dot( cross( M_t[0], M_t[1] ), M_t[2] );
    mat3 adjugate = mat3( cross( M_t[1], M_t[2] ), cross( M_t[2], M_t[0] ), cross( M_t[0], M_t[1] ) );
    return adjugate / det;
}






mat3 cotangent_frame( vec3 N, vec3 p, vec2 uv )
{
    // get edge vectors of the pixel triangle
    vec3 dp1 = dFdx( p );
    vec3 dp2 = dFdy( p );
    vec2 duv1 = dFdx( uv );
    vec2 duv2 = dFdy( uv );
    // solve the linear system
    vec3 dp2perp = cross( dp2, N );
    vec3 dp1perp = cross( N, dp1 );
    vec3 T = dp2perp * duv1.x + dp1perp * duv2.x;
    vec3 B = dp2perp * duv1.y + dp1perp * duv2.y;
    // construct a scale-invariant frame
    float invmax = inversesqrt( max( dot(T,T), dot(B,B) ) );
    return mat3( T * invmax, B * invmax, N );
}


const int WITH_NORMALMAP_UNSIGNED = 2;
const int WITH_NORMALMAP_2CHANNEL = 4;
const int WITH_NORMALMAP_GREEN_UP = 8;
const int MODE_STANDARD = 0;
const int MODE_NORMAL = 1;
const int MODE_NORMAL_MAP = 2;
const int MODE_POSITION_VARIATION_X = 3;
const int MODE_POSITION_VARIATION_Y = 4;
const int MODE_UV_VARIATION_X = 5;
const int MODE_UV_VARIATION_Y = 6;
const int MODE_UV = 7;
const int MODE_POS_WORLD = 8;
const int MODE_POS_VIEW = 9;

vec3 perturb_normal( vec3 N, vec3 V, vec2 texcoord )
{
    // assume N, the interpolated vertex normal and
    // V, the view vector (vertex to eye)
    vec3 map = texture2D( uNormalTexture, texcoord ).xyz;

    if ((uUseNormal & WITH_NORMALMAP_UNSIGNED) != 0)
    {
        map = map * 255./127. - 128./127.;
    }
    if ((uUseNormal & WITH_NORMALMAP_2CHANNEL) != 0)
    {
        map.z = sqrt( 1. - dot( map.xy, map.xy ) );
    }
    if ((uUseNormal & WITH_NORMALMAP_GREEN_UP) != 0)
    {
        map.y = -map.y;
    }
    mat3 TBN = cotangent_frame( N, -V, texcoord );
    return normalize( TBN * map );
}


void main()
{
  vec3 N = normalize(vViewSpaceNormal);
  vec3 L = uLightDir;
  vec3 V = normalize(-vViewSpacePosition);
  vec3 H = normalize(L+V);

  if (uUseNormal != 0)
  {
      N = perturb_normal( N, -V, vTexCoords );
  }
  
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

  if (uRenderMode == MODE_STANDARD)
  {
      fColor = LINEARtoSRGB(color);
  }
  else if (uRenderMode == MODE_NORMAL)
  {
      fColor = N;
  }
  else if (uRenderMode == MODE_NORMAL_MAP)
  {
      fColor = texture(uNormalTexture, vTexCoords).rgb;
  }
  else if (uRenderMode == MODE_POSITION_VARIATION_X)
  {
    vec3 dp = dFdx( vViewSpacePosition );

    fColor = dp*10.;
  }
  else if (uRenderMode == MODE_POSITION_VARIATION_Y)
  {
    vec3 dp = dFdy( vViewSpacePosition );

    fColor = dp*10.;
  }
  else if (uRenderMode == MODE_UV_VARIATION_X)
  {
    vec2 dp = dFdx( vTexCoords );

    fColor = vec3(dp, 0)*100.;
  }
  else if (uRenderMode == MODE_UV_VARIATION_Y)
  {
    vec2 dp = dFdy( vTexCoords );

    fColor = vec3(dp, 0)*100.;
  }
  else if (uRenderMode == MODE_UV)
  {
    fColor = vec3(vTexCoords, 0);
  }
  else if (uRenderMode == MODE_POS_WORLD)
  {
    fColor = vWorldSpacePosition;
  }
  else if (uRenderMode == MODE_POS_VIEW)
  {
    fColor = -vViewSpacePosition;
  }
}
