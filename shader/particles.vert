#version 460

const float PI = 3.141592653589;

layout (location = 0) in vec3 VertexPosition;
layout (location = 1) in vec3 VertexVelocity;
layout (location = 2) in float VertexAge;

uniform int Pass;

out vec3 Position;
out vec3 Velocity;
out float Age;


out float Transparency;
out vec2 TexCoord;

uniform mat4 ProjectionMatrix;
uniform mat4 ModelViewMatrix;


uniform sampler1D RandomTexture;
uniform vec3 ParticleAcceleration;
uniform vec3 EmitterPosition;
uniform mat3 EmitterDirection;
uniform float ParticleStartSize;
uniform float Time;
uniform float DeltaTime;
uniform float ParticleLifetime;


const vec3 offsets[] = vec3[](vec3(-0.5, -0.5, 0), vec3(0.5, -0.5, 0), vec3(0.5, 0.5, 0),
                              vec3(-0.5, -0.5, 0), vec3(0.5, 0.5, 0), vec3(-0.5, 0.5, 0)
);

const vec2 texCoords[] = vec2[](vec2(0, 0), vec2(1, 0), vec2(1, 1),
                              vec2(0, 0), vec2(1, 1), vec2(0, 1)
);

vec3 randomStartingVelocity() {
    float velocity = mix(0.05, 0.2, texelFetch(RandomTexture, 2 * gl_VertexID, 0).r); // Random velocity between 0.05 and 0.2
    return EmitterDirection * vec3(0, velocity, 0); // Float upwards
}

vec3 randomStartingPosition() {
    float offsetX = mix(-0.5, 0.5, texelFetch(RandomTexture, 2 * gl_VertexID + 1, 0).r);
    float offsetZ = mix(-0.5, 0.5, texelFetch(RandomTexture, 2 * gl_VertexID + 2, 0).r);
    return EmitterPosition + EmitterDirection * vec3(offsetX, 0, offsetZ);
}

void update() {
    Age = VertexAge + DeltaTime;
    if (VertexAge < 0 ||  VertexAge > ParticleLifetime) {
        Position = randomStartingPosition();
        Velocity = randomStartingVelocity();

        if (VertexAge>ParticleLifetime) {
            Age = (VertexAge - ParticleLifetime) + DeltaTime;
        }
    } else {
        Position = VertexPosition + VertexVelocity * DeltaTime;
        Velocity = VertexVelocity + ParticleAcceleration * DeltaTime;

        // Increase speed over time (As particles rise)
        Velocity.y += DeltaTime * 0.1;

        // Fire swirl 
        vec3 particleCenter = VertexPosition - EmitterPosition; // Particle system center
        vec3 swirl = vec3(-particleCenter.z, 0, particleCenter.x) * 0.2; // Swirl strength
        Velocity += swirl * DeltaTime; 

    }
}

void render() {
    Transparency = 0.0;
    vec3 cameraPosition = vec3(0.0);
    if (VertexAge >= 0.0) {
        // Shrink particles over time
        float particleLife = VertexAge / ParticleLifetime;
        float particleSize = mix(ParticleStartSize, 0.0, particleLife * 0.7);
        // Set particle positions 
        cameraPosition = (ModelViewMatrix * vec4(VertexPosition, 1.0)).xyz + offsets[gl_VertexID] * particleSize;

        // Fade out particles over time randomly
        float randomFade = mix(-0.2, 0.2, texelFetch(RandomTexture, 2 * gl_VertexID + 3, 0).r); // Random fade amount
        float minFade = 0.4 * (1.0 - particleLife); // Minimum fade based on particle life
        Transparency = clamp(1.0 - particleLife + randomFade, minFade, 1.0);

    }
    TexCoord = texCoords[gl_VertexID];
    gl_Position = ProjectionMatrix * vec4(cameraPosition, 1.0);
}

void main()
{
    if (Pass == 1) {
        update();
    } else {
        render();
    }

}
