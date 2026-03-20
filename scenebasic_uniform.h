#ifndef SCENEBASIC_UNIFORM_H
#define SCENEBASIC_UNIFORM_H

#include "helper/scene.h"

#include <glad/glad.h>
#include "helper/glslprogram.h"

#include <glm/gtc/matrix_transform.hpp>
#include <glm/glm.hpp>

#include "helper/plane.h"
#include "helper/objmesh.h"
#include <GLFW/glfw3.h>
#include "helper/skybox.h"

#include "helper/particleutils.h"
#include "helper/frustum.h"
#include "helper/noisetex.h"

class SceneBasic_Uniform : public Scene
{
private:
	Plane plane;
	std::unique_ptr<ObjMesh> barrel, roof, barrier;
	SkyBox skybox;
	GLuint vaoHandle;
	GLuint hdrFBO, hdrTexture, quad, shadowFBO;
	GLuint blurFBO, tex1, tex2;
	GLuint linearSampler, nearestSampler, shadowSampler;
	int bloomBufferWidth, bloomBufferHeight;

	GLSLProgram prog;
	GLSLProgram skyboxProg;
	GLSLProgram particleProg;
	float angle;
	float deltaTime;
	float tPrev;

	// Particles
	GLuint posBuffer[2], velBuffer[2], age[2];
	GLuint particleArray[2];
	GLuint feedback[2];
	GLuint drawBuffer;
	int numberOfParticles;
	float particleLifetime;
	float particleSize;

	// Shadows
	Frustum shadowFrustum;
	glm::mat4 shadowPV;
	void shadowPass();
	void debugShadowMap();
	GLuint shadowDepthTex;


	void setMatrices();

	void compile();
	void setupFBO();
	void initParticleBuffers();
	void pass1(); void pass2(); void pass3(); void pass4(); void pass5();
	float gauss(float, float);
	void drawScene();
	void drawParticles();
	void computeLogAveLuminance();
	void userInput(GLFWwindow* WindowIn);


	// Textures
	GLuint floorDiffuseTexture;
	GLuint floorNormalTexture;

	GLuint wallDiffuseTexture;
	GLuint wallNormalTexture;

	GLuint damageDiffuseTexture;
	GLuint damageNormalTexture;

	GLuint barrelDiffuseTexture;
	GLuint barrelNormalTexture;

	GLuint barrierDiffuseTexture;
	GLuint barrierNormalTexture;

	GLuint skyboxTexture;
	GLuint noiseTexture;

	GLuint particleTexture;

public:
	SceneBasic_Uniform();

	void initScene();
	void update(float t, GLFWwindow* window);
	void render();
	void resize(int, int);

};

#endif // SCENEBASIC_UNIFORM_H
