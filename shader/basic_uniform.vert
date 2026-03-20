#version 460

layout (location = 0) in vec3 VertexPosition;
layout (location = 1) in vec3 VertexNormal;
layout (location = 2) in vec2 VertexTexCoord;
layout (location = 3) in vec4 VertexTangent;

out vec3 Position;
out vec3 Normal;
out vec2 TexCoord;
out vec4 ShadowCoord;


// Uniforms
uniform mat4 ModelViewMatrix;
uniform mat3 NormalMatrix;
uniform mat4 ProjectionMatrix;
uniform mat4 MVP;
uniform mat4 LightProjectionMatrix;


void main()
{
	Normal = normalize(NormalMatrix * VertexNormal);
	vec3 tangent = normalize(NormalMatrix * vec3(VertexTangent));
	vec3 binormal = normalize(cross(Normal, tangent) * VertexTangent.w);
	
	mat3 toObjectSpace = mat3(tangent, binormal, Normal);

	TexCoord = VertexTexCoord;
	ShadowCoord = LightProjectionMatrix * vec4(VertexPosition, 1.0);

	Position = (ModelViewMatrix * vec4(VertexPosition, 1.0)).xyz;
	gl_Position = MVP * vec4(VertexPosition, 1.0);
}