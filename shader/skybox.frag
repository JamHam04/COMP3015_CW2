#version 460

uniform samplerCube SkyBoxTexture;
in vec3 VecPosition;
out vec4 FragColor;

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
    
    vec3 color = texture(SkyBoxTexture, normalize(flippedVec)).rgb;
    color = pow(color, vec3(1.0/2.2)); // gamma correction

    
    FragColor = vec4(color * 0.5, 1.0);
}