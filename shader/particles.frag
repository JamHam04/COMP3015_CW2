#version 460


in vec2 TexCoord;
layout (location = 0) out vec4 FragColor;

uniform sampler2D ParticleTexture;
in float Transparency;




void main()
{
	FragColor = texture(ParticleTexture, TexCoord);
	FragColor = vec4(mix(vec3(0.0), FragColor.xyz, Transparency), FragColor.a);
	FragColor.a *= Transparency;

}
