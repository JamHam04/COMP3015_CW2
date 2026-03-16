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

class SceneBasic_Uniform : public Scene
{
private:
    Plane plane;
	std::unique_ptr<ObjMesh> barrel, roof, barrier;
	SkyBox skybox;
    GLuint vaoHandle;
	GLuint hdrFBO, hdrTexture, quad;
	GLuint blurFBO, tex1, tex2; 
	GLuint linearSampler, nearestSampler;
	int bloomBufferWidth, bloomBufferHeight;

    GLSLProgram prog;
	GLSLProgram skyboxProg;
    float angle;
    float deltaTime;
	float tPrev;
	void setMatrices();

    void compile();
	void setupFBO();
	void pass1(); void pass2(); void pass3(); void pass4(); void pass5();
	float gauss(float, float);
	void drawScene();
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

public:
    SceneBasic_Uniform();

    void initScene();
    void update( float t , GLFWwindow* window);
    void render();
    void resize(int, int);
    
};

#endif // SCENEBASIC_UNIFORM_H
