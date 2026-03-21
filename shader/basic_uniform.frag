#version 460

in vec3 Position;
in vec3 Normal;
in vec2 TexCoord;


layout (binding = 0) uniform sampler2D HDRTex;
layout (binding = 1) uniform sampler2D BlurTex1;
layout (binding = 2) uniform sampler2D BlurTex2;

// HDR
uniform int Pass;
uniform float AvgLum;
uniform float Exposure = 0.15;
uniform float White = 0.928;

// Bloom
uniform float LumThresh;
uniform float PixOffset[10] = float[](0.0, 1.0, 2.0, 3.0, 4.0, 5.0, 6.0, 7.0, 8.0, 9.0);
uniform float Weight[10];

// Shadows
uniform sampler2DShadow ShadowMap;
in vec4 ShadowCoord;


layout (location = 0) out vec4 FragColor;

uniform mat3 rgb2xyz = mat3(
    0.4124564, 0.2126729, 0.0193339,
    0.3572769, 0.7151522, 0.1191920,
    0.1804375, 0.0721750, 0.9503041
);

uniform mat3 xyz2rgb = mat3(
    3.2404542, -0.9692660, 0.0556434,
    -1.5371385, 1.8760108, -0.2040259,
    -0.4985314, 0.0415560, 1.0572252
);

float luminance(vec3 color) {
    return 0.2126 * color.r + 0.7152 * color.g + 0.0722 * color.b;
}

uniform int NumLights;
uniform bool useMixTexture;
uniform bool showShadows;

uniform struct LightInfo {
    vec4 Position;
    vec3 La; // ambient
    vec3 Ld; // diffuse
    vec3 L; // intensity
} Lights[3];

uniform struct MaterialInfo {
    vec3 Ka; // ambient
    vec3 Kd; // diffuse
    vec3 Ks; // specular
    float Shininess;
} Material;

uniform struct TextureInfo {
    sampler2D diffuseTexture;
    sampler2D normalTexture;
    sampler2D mixDiffuseTexture;
    sampler2D mixNormalTexture;
} Textures;

uniform struct FogInfo {
    vec3 Color;
    float minDist;
    float maxDist;
} Fog;

vec3 blinnPhong(LightInfo light, vec3 position, vec3 normal, vec3 texture) {
    vec3 diffuse = vec3(0), specular = vec3(0);


    vec3 ambient = light.La * texture;

    vec3 s = normalize(light.Position.xyz - position);

    float sDotN = max(dot(s, normal), 0.0);
    diffuse = light.Ld * texture * sDotN;
    if (sDotN > 0.0) {
        vec3 v = normalize(-position);
        vec3 h = normalize(s + v);
        specular = Material.Ks * pow(max(dot(h, normal), 0.0), Material.Shininess);
    }

    return ambient + (diffuse + specular) * light.L;
}

// HDR
vec4 Pass1() {
    vec3 color = vec3(0);

    // Fog
    float distance = abs(Position.z);
    float fogFactor = clamp((Fog.maxDist - distance) / (Fog.maxDist - Fog.minDist), 0.0, 1.0);

    // Normal mapping
    vec3 diffuseTex = texture(Textures.diffuseTexture, TexCoord).rgb;
    vec3 normalTex = texture(Textures.normalTexture, TexCoord).rgb;
    normalTex = 2.0 * normalTex - 1.0;

    vec3 finalDiffuseTex = diffuseTex;
    vec3 finalNormalTex = normalTex;


    // Mixing Textures
    if (useMixTexture) {
        vec4 mixDiffuseTex = texture(Textures.mixDiffuseTexture, TexCoord);
        vec3 mixNormalTex = texture(Textures.mixNormalTexture, TexCoord).rgb;
  
        mixNormalTex = 2.0 * mixNormalTex - 1.0;

        float mixFactor = mixDiffuseTex.a;
        finalDiffuseTex = mix(diffuseTex, mixDiffuseTex.rgb, mixFactor);
        finalNormalTex = mix(normalTex, mixNormalTex, mixFactor);
        
    }


    // Lighting
    for (int i = 0; i < NumLights; i++) {
        float shadow = 1.0;

        if(i == 0 && showShadows) {
            if (ShadowCoord.z >= 0) shadow = textureProj(ShadowMap, ShadowCoord);
        }

        color += blinnPhong(Lights[i], Position, normalize(finalNormalTex), finalDiffuseTex) * shadow;
    }
    // Shadow Mapping

    color = mix(Fog.Color, color, fogFactor);
    return vec4(color, 1.0);

}

vec4 Pass2() {
    vec4 val = texture(HDRTex, TexCoord);

    if (luminance(val.rgb) > LumThresh)
        return val;
    else
        return vec4(0.0);

}
vec4 Pass3() {
    float dx = 1.0 / (textureSize(BlurTex1, 0)).y;
    vec4 sum = texture(BlurTex1, TexCoord) * Weight[0];
    for (int i = 1; i < 10; i++) {
        sum += texture(BlurTex1, TexCoord + vec2(0.0, PixOffset[i]) * dx) * Weight[i];
        sum += texture(BlurTex1, TexCoord - vec2(0.0, PixOffset[i]) * dx) * Weight[i];
    }
    return sum;

}

vec4 Pass4() {
    float dy = 1.0 / (textureSize(BlurTex2, 0)).x;
    vec4 sum = texture(BlurTex2, TexCoord) * Weight[0];
    for (int i = 1; i < 10; i++) {
        sum += texture(BlurTex2, TexCoord + vec2(PixOffset[i], 0.0) * dy) * Weight[i];
        sum += texture(BlurTex2, TexCoord - vec2(PixOffset[i], 0.0) * dy) * Weight[i];
    }
    return sum;

}



// Tonemapping
vec4 Pass5() {
    vec4 color = texture(HDRTex, TexCoord);
    vec3 bloom = texture(BlurTex1, TexCoord).rgb;

    vec3 xyzCol = rgb2xyz * color.rgb;
    float xyzSum = xyzCol.x + xyzCol.y + xyzCol.z;

    vec3 xyYCol = vec3(xyzCol.x / xyzSum, xyzCol.y / xyzSum, xyzCol.y);

    float Lum = xyYCol.z;
    float lumFactor = Exposure * (Lum / AvgLum);
    lumFactor = (lumFactor * (1.0 + lumFactor / (White * White))) / (1.0 + lumFactor);



    xyzCol = xyzCol * (lumFactor / Lum);

    vec3 toneMappedColor = xyz2rgb * xyzCol;

    toneMappedColor += bloom;

    return vec4(toneMappedColor, color.a);
}

float Gamma = 2.2f;

void main()
{
    if(Pass == 1) FragColor = Pass1();
    else if(Pass == 2) FragColor = Pass2();
    else if(Pass == 3) FragColor = Pass3();
    else if(Pass == 4) FragColor = Pass4();
    //else if(Pass == 5) FragColor = Pass5();
    else if(Pass == 5) FragColor = vec4(pow(vec3(Pass5()), vec3(1.0 / Gamma)), 1.0);


}
