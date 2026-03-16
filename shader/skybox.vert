#version 460

layout (location = 0) in vec3 VertexPosition;



out vec3 VecPosition;




// Uniforms
uniform mat4 MVP;

void main()
{
	VecPosition = VertexPosition;

	gl_Position = MVP * vec4(VertexPosition, 1.0);
}