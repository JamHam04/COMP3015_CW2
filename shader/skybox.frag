#version 460

#define PI 3.14159265

uniform samplerCube SkyBoxTexture;
uniform sampler2D NoiseTexture;

uniform vec3 CloudColor = vec3(0.3, 0.3, 0.3);

in vec3 VecPosition;
out vec4 FragColor;

uniform float Time;

void main()
{
    vec3 flippedVec = VecPosition;
    
    // Flip skybox walls and rotate top/bottom faces
    if (abs(VecPosition.y) > abs(VecPosition.x) && abs(VecPosition.y) > abs(VecPosition.z)) {
        // Top/bottom faces (Keep y, flip z)
        flippedVec = vec3(VecPosition.x, VecPosition.y, -VecPosition.z);
    } else {
        // Side faces - flip Y
        flippedVec = vec3(VecPosition.x, -VecPosition.y, VecPosition.z);
    }
    
    vec3 skyboxColor = texture(SkyBoxTexture, normalize(flippedVec)).rgb;

    // Convert VecPosition
    vec3 dir = normalize(VecPosition);
    
    vec2 noiseUV = dir.xz * 0.5 + 0.5;

    // Animate noise
    noiseUV = noiseUV + vec2(0.02, 0.01) * Time;

    float noise = texture(NoiseTexture, noiseUV).r; 

    float t = smoothstep(0.4, 0.8, noise); 
    
    vec3 color = mix(skyboxColor, CloudColor, t); 
    color = pow(color, vec3(1.0/2.2)); // gamma correction
    
    FragColor = vec4(color * 0.5, 1.0);
}