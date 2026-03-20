#include "scenebasic_uniform.h"

#include <cstdio>
#include <cstdlib>

#include <string>
using std::string;

#include <iostream>
using std::cerr;
using std::endl;

#include "helper/glutils.h"
#include "helper/texture.h"	

using glm::vec3;
using glm::vec4;
using glm::mat3;
using glm::mat4;

vec3 cameraPos(0.0f, 4.0f, 6.0f);
vec3 cameraTarget(0.0f, 0.0f, -1.0f);
vec3 cameraUp(0.0f, 1.0f, 0.0f);
float cameraSpeed = 5.0f;

float cameraYaw = -90.0f;
float cameraPitch = 0.0f;
bool firstMoved = true;

float cameraLastX;
float cameraLastY;

SceneBasic_Uniform::SceneBasic_Uniform() : plane(50.0f, 50.0f, 1, 1), skybox(100.0f),
particleLifetime(4.0f), numberOfParticles(1000), particleSize(0.5f), drawBuffer(1)
{
	// Load models
	barrel = ObjMesh::load("media/model/barrel_stove_4k.obj", true);
	roof = ObjMesh::load("media/model/Broken_Wall.obj", true);
	barrier = ObjMesh::load("media/model/concrete_road_barrier_02_4k.obj", true);
}

void SceneBasic_Uniform::initScene()
{
	glEnable(GL_DEPTH_TEST);

	// disable log spam
	glDebugMessageControl(GL_DONT_CARE, GL_DONT_CARE, GL_DEBUG_SEVERITY_NOTIFICATION, 0, nullptr, GL_FALSE);

	// Camera View
	cameraLastX = width / 2.0f;
	cameraLastY = height / 2.0f;

	compile();
	model = glm::mat4(1.0f);
	view = glm::lookAt(cameraPos, cameraPos + cameraTarget, cameraUp);

	projection = glm::perspective(glm::radians(70.0f), (float)width / height, 0.3f, 100.0f);

	setupFBO();
	glActiveTexture(GL_TEXTURE5);
	glBindTexture(GL_TEXTURE_2D, shadowDepthTex);

	// Setup quad 
	GLfloat verts[] = {
	-1.0f, -1.0f, 0.0f, 1.0f, -1.0f, 0.0f, 1.0f, 1.0f, 0.0f,
	-1.0f, -1.0f, 0.0f, 1.0f, 1.0f, 0.0f, -1.0f, 1.0f, 0.0f
	};
	GLfloat tc[] = {
		0.0f, 0.0f, 1.0f, 0.0f, 1.0f, 1.0f,
		0.0f, 0.0f, 1.0f, 1.0f, 0.0f, 1.0f
	};

	// Create quad buffers
	unsigned int handle[2];
	glGenBuffers(2, handle);
	glBindBuffer(GL_ARRAY_BUFFER, handle[0]);
	glBufferData(GL_ARRAY_BUFFER, 6 * 3 * sizeof(GLfloat), verts, GL_STATIC_DRAW);

	glBindBuffer(GL_ARRAY_BUFFER, handle[1]);
	glBufferData(GL_ARRAY_BUFFER, 6 * 2 * sizeof(GLfloat), tc, GL_STATIC_DRAW);

	glGenVertexArrays(1, &quad);
	glBindVertexArray(quad);

	glBindBuffer(GL_ARRAY_BUFFER, handle[0]);
	glVertexAttribPointer((GLuint)0, 3, GL_FLOAT, GL_FALSE, 0, 0);
	glEnableVertexAttribArray(0);

	glBindBuffer(GL_ARRAY_BUFFER, handle[1]);
	glVertexAttribPointer((GLuint)2, 2, GL_FLOAT, GL_FALSE, 0, 0);
	glEnableVertexAttribArray(2);

	glBindVertexArray(0);

	prog.setUniform("LumThresh", 1.7f);

	// Bloom
	float weights[10], sum, sigma2 = 25.0f;
	weights[0] = gauss(0, sigma2);
	sum = weights[0];
	for (int i = 1; i < 10; i++) {
		weights[i] = gauss(float(i), sigma2);
		sum += 2 * weights[i];
	}
	// Normalize the weights and set the uniform
	for (int i = 0; i < 10; i++) {
		std::stringstream uniName;
		uniName << "Weight[" << i << "]";
		float val = weights[i] / sum;
		prog.setUniform(uniName.str().c_str(), val);
	}
	// Set up sampler objects 
	GLuint samplers[3];
	glGenSamplers(3, samplers);
	linearSampler = samplers[0];
	nearestSampler = samplers[1];
	shadowSampler = samplers[2];
	GLfloat border[] = { 0.0f,0.0f,0.0f,0.0f };
	// nearest sampler
	glSamplerParameteri(nearestSampler, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glSamplerParameteri(nearestSampler, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glSamplerParameteri(nearestSampler, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
	glSamplerParameteri(nearestSampler, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
	glSamplerParameterfv(nearestSampler, GL_TEXTURE_BORDER_COLOR, border);
	// linear sampler
	glSamplerParameteri(linearSampler, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glSamplerParameteri(linearSampler, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glSamplerParameteri(linearSampler, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
	glSamplerParameteri(linearSampler, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
	glSamplerParameterfv(linearSampler, GL_TEXTURE_BORDER_COLOR, border);

	glBindSampler(0, nearestSampler);
	glBindSampler(1, nearestSampler);
	glBindSampler(2, nearestSampler);
	glSamplerParameteri(shadowSampler, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glSamplerParameteri(shadowSampler, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glSamplerParameteri(shadowSampler, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
	glSamplerParameteri(shadowSampler, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
	glSamplerParameteri(shadowSampler, GL_TEXTURE_COMPARE_MODE, GL_COMPARE_REF_TO_TEXTURE);
	glSamplerParameteri(shadowSampler, GL_TEXTURE_COMPARE_FUNC, GL_LEQUAL);

	// Bind shadow
	glBindSampler(5, shadowSampler);

	// Light properties
	prog.setUniform("NumLights", 3); // Number of lights

	prog.setUniform("Lights[0].L", vec3(0.35f, 0.38f, 0.45f)); // Light intensity
	prog.setUniform("Lights[0].La", vec3(0.05f)); // Ambient light intensity
	prog.setUniform("Lights[0].Ld", vec3(0.5f)); // Diffuse light intensity

	prog.setUniform("Lights[1].L", vec3(0.12f, 0.12f, 0.13f)); // Light intensity
	prog.setUniform("Lights[1].La", vec3(0.03f)); // Ambient light intensity
	prog.setUniform("Lights[1].Ld", vec3(0.25f)); // Diffuse light intensity


	// Fire light (inside barrel)
	prog.setUniform("Lights[2].L", vec3(1.0f, 0.4f, 0.2f)); // Light intensity
	prog.setUniform("Lights[2].La", vec3(0.2f, 0.05f, 0.0f)); // Ambient light intensity
	prog.setUniform("Lights[2].Ld", vec3(1.0f, 0.4f, 0.1f)); // Diffuse light intensity


	// Fog properties
	prog.setUniform("Fog.maxDist", 20.0f);
	prog.setUniform("Fog.minDist", 1.0f);
	prog.setUniform("Fog.Color", vec3(0.03f, 0.05f, 0.08f));

	// Load Textures
	floorDiffuseTexture = Texture::loadTexture("media/texture/asphalt_01_diff_4k.jpg");
	wallDiffuseTexture = Texture::loadTexture("media/texture/rough_plaster_03_diff_4k.jpg");

	damageDiffuseTexture = Texture::loadTexture("media/texture/Damage.png");
	damageNormalTexture = Texture::loadTexture("media/texture/Damage_Normal.png");

	floorNormalTexture = Texture::loadTexture("media/texture/asphalt_01_nor_gl_4k.jpg");
	wallNormalTexture = Texture::loadTexture("media/texture/rough_plaster_03_nor_gl_4k.jpg");

	barrelDiffuseTexture = Texture::loadTexture("media/texture/barrel_stove_diff_4k.jpg");
	barrelNormalTexture = Texture::loadTexture("media/texture/barrel_stove_nor_gl_4k.jpg");

	barrierDiffuseTexture = Texture::loadTexture("media/texture/concrete_road_barrier_02_diff_4k.jpg");
	barrierNormalTexture = Texture::loadTexture("media/texture/concrete_road_barrier_02_nor_gl_4k.jpg");

	skyboxTexture = Texture::loadHdrCubeMap("media/texture/cube/night/n");


	// Particle buffers
	
	particleTexture = Texture::loadTexture("media/texture/fire.png");


	glActiveTexture(GL_TEXTURE1);
	ParticleUtils::createRandomTex1D(numberOfParticles * 3);

	initParticleBuffers();

	particleProg.use();
	particleProg.setUniform("RandomTexture", 1);
	particleProg.setUniform("ParticleTexture", 4);

	particleProg.setUniform("ParticleLifetime", particleLifetime);
	particleProg.setUniform("ParticleStartSize", particleSize);
	particleProg.setUniform("ParticleAcceleration", vec3(0.0f, 0.1f, 0.0f));

	particleProg.setUniform("EmitterPosition", vec3(0.0f, 1.5f, 4.0f)); // Emit from inside barrel
	particleProg.setUniform("EmitterDirection", ParticleUtils::makeArbitraryBasis(vec3(0.0f, 1.0f, 0.0f))); // Emit upwards

	prog.use();	

	// Noise
	noiseTexture = NoiseTex::generate2DTex(3.0f);


}

void SceneBasic_Uniform::compile()
{
	try {
		prog.compileShader("shader/basic_uniform.vert");
		prog.compileShader("shader/basic_uniform.frag");
		prog.link();
		prog.use();

		skyboxProg.compileShader("shader/skybox.vert");
		skyboxProg.compileShader("shader/skybox.frag");
		skyboxProg.link();

		// Particle shader
		particleProg.compileShader("shader/particles.vert");
		particleProg.compileShader("shader/particles.frag");
		GLuint progHandle = particleProg.getHandle();
		const char* feedbackOutputs[] = { "Position", "Velocity", "Age" };
		glTransformFeedbackVaryings(progHandle, 3, feedbackOutputs, GL_SEPARATE_ATTRIBS);

		particleProg.link();

	}
	catch (GLSLProgramException& e) {
		cerr << e.what() << endl;
		exit(EXIT_FAILURE);
	}
}

void SceneBasic_Uniform::update(float t, GLFWwindow* window)
{
	// Time
	deltaTime = t - tPrev;



	if (tPrev == 0.0f) {
		deltaTime = 0.0f;
	}
	tPrev = t;

	// Handle user input for camera movement
	userInput(window);

	// Lock mouse movement
	static bool cursorDisabled = false;
	if (!cursorDisabled) {
		glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
		cursorDisabled = true;
	}

}
void SceneBasic_Uniform::render()
{
	shadowPass();
	pass1();
	computeLogAveLuminance();
	pass2();
	pass3();
	pass4();
	pass5();

	glBindSampler(0, 0);
	glBindSampler(1, 0);
}

// Shadow pass
void SceneBasic_Uniform::shadowPass()
{

	vec3 lightPos = vec3(0.0f, 20.0f, 0.0f); // Directly above scene center
	vec3 target = vec3(0.0f, 0.0f, 0.0f);    // Looking straight down at scene center
	vec3 up = vec3(0.0f, 0.0f, -1.0f);       // Up vector perpendicular to light direction

	shadowFrustum.orient(lightPos, target, up);
	shadowFrustum.setPerspective(60.0f, 1.0f, 1.0f, 50.0f);

	mat4 shadowBias = mat4(
		0.5f, 0.0f, 0.0f, 0.0f,
		0.0f, 0.5f, 0.0f, 0.0f,
		0.0f, 0.0f, 0.5f, 0.0f,
		0.5f, 0.5f, 0.5f, 1.0f
	);
	shadowPV = shadowBias * shadowFrustum.getProjectionMatrix() * shadowFrustum.getViewMatrix();

	glBindFramebuffer(GL_FRAMEBUFFER, shadowFBO);
	glViewport(0, 0, 2048, 2048);
	glClear(GL_DEPTH_BUFFER_BIT);
	view = shadowFrustum.getViewMatrix();
	projection = shadowFrustum.getProjectionMatrix();

	glDisable(GL_CULL_FACE);
	glEnable(GL_POLYGON_OFFSET_FILL);
	glPolygonOffset(4.0f, 16.0f); 
	drawScene();    
	glDisable(GL_POLYGON_OFFSET_FILL);
	glFlush();
	


}

// HDR
void SceneBasic_Uniform::pass1()
{
	prog.use();
	prog.setUniform("Pass", 1);

	glActiveTexture(GL_TEXTURE5);
	glBindTexture(GL_TEXTURE_2D, shadowDepthTex);
	glBindSampler(5, shadowSampler);
	prog.setUniform("ShadowMap", 5);


	glViewport(0, 0, width, height);
	glBindFramebuffer(GL_FRAMEBUFFER, hdrFBO);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	glEnable(GL_DEPTH_TEST);
	view = glm::lookAt(cameraPos, cameraPos + cameraTarget, cameraUp);
	projection = glm::perspective(glm::radians(60.0f), (float)width / height, 0.3f, 100.0f);

	drawScene();
	drawParticles(); // Draw fire particles 
}

// Bloom
void SceneBasic_Uniform::pass2()
{
	glDisable(GL_DEPTH_TEST);
	glDisable(GL_CULL_FACE);

	prog.setUniform("Pass", 2);
	glBindFramebuffer(GL_FRAMEBUFFER, blurFBO);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, tex1, 0);
	glViewport(0, 0, bloomBufferWidth, bloomBufferHeight);

	glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
	glClear(GL_COLOR_BUFFER_BIT);

	model = mat4(1.0f);
	view = mat4(1.0f);
	projection = mat4(1.0f);

	setMatrices();

	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, hdrTexture);

	glBindVertexArray(quad);
	glDrawArrays(GL_TRIANGLES, 0, 6);

	glEnable(GL_DEPTH_TEST); // ENABLE DEPTH FOR SHADOWS
	glEnable(GL_CULL_FACE);
}

// Blur (vertical)
void SceneBasic_Uniform::pass3()
{
	glDisable(GL_DEPTH_TEST);
	glDisable(GL_CULL_FACE);

	prog.setUniform("Pass", 3);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, tex2, 0);

	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, tex1);

	glActiveTexture(GL_TEXTURE1);
	glBindTexture(GL_TEXTURE_2D, tex1);

	glBindVertexArray(quad);
	glDrawArrays(GL_TRIANGLES, 0, 6);

	glEnable(GL_DEPTH_TEST); 
	glEnable(GL_CULL_FACE);
}

// Blur (horizontal)
void SceneBasic_Uniform::pass4()
{
	glDisable(GL_DEPTH_TEST);
	glDisable(GL_CULL_FACE);

	prog.setUniform("Pass", 4);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, tex1, 0);

	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, tex2);

	glActiveTexture(GL_TEXTURE2);
	glBindTexture(GL_TEXTURE_2D, tex2);

	glBindVertexArray(quad);
	glDrawArrays(GL_TRIANGLES, 0, 6);

	glEnable(GL_DEPTH_TEST);
	glEnable(GL_CULL_FACE);
}

void SceneBasic_Uniform::drawScene() {

	// SKYBOX
	skyboxProg.use();

	mat4 skyboxView = mat4(mat3(view));
	mat4 vp = projection * skyboxView;
	skyboxProg.setUniform("MVP", vp);

	skyboxProg.setUniform("SkyBoxTexture", 0);
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_CUBE_MAP, skyboxTexture);

	skyboxProg.setUniform("NoiseTexture", 1);
	glActiveTexture(GL_TEXTURE1);
	glBindTexture(GL_TEXTURE_2D, noiseTexture);

	skybox.render();

	prog.use();
	// Set light position
	vec4 lightPos2 = vec4(8.0f, 3.0f, 0.0f, 1.0f);
	vec4 fireLightPos = vec4(0.0f, 1.5f, 4.0f, 1.0f); // Inside barrel



	//prog.setUniform("Lights[0].Position", view * lightPos);

	vec4 lightPos = vec4(0.0f, 20.0f, 0.0f, 1.0f);
	prog.setUniform("Lights[0].Position", view * lightPos);

	prog.setUniform("Lights[1].Position", view * lightPos2);
	prog.setUniform("Lights[2].Position", view * fireLightPos);


	// Animte fire light inside barrel
	//float fireIntensity = 1.0f + 0.75f * sin(tPrev * 5.0f); // Flicker 
	//prog.setUniform("Lights[2].L", vec3(0.0f));
	//prog.setUniform("Lights[2].L", vec3(fireIntensity) * 0.35f); // Update fire light intensity

	// Set material properties
	vec3 diffuseColor = vec3(0.5f, 0.0f, 0.0f);
	vec3 specularColor = vec3(1.0f, 1.0f, 1.0f);
	vec3 ambientColor = vec3(0.2f, 0.0f, 0.0f);

	prog.setUniform("Material.Kd", diffuseColor);
	prog.setUniform("Material.Ks", specularColor);
	prog.setUniform("Material.Ka", ambientColor);
	prog.setUniform("Material.Shininess", 75.0f);

	prog.setUniform("showShadows", true);

	// FLOOR
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, floorDiffuseTexture);

	glActiveTexture(GL_TEXTURE1);
	glBindTexture(GL_TEXTURE_2D, floorNormalTexture);

	glActiveTexture(GL_TEXTURE2);
	glBindTexture(GL_TEXTURE_2D, damageDiffuseTexture);

	glActiveTexture(GL_TEXTURE3);
	glBindTexture(GL_TEXTURE_2D, damageNormalTexture);

	prog.setUniform("useMixTexture", true);
	prog.setUniform("Textures.diffuseTexture", 0);
	prog.setUniform("Textures.normalTexture", 1);
	prog.setUniform("Textures.mixDiffuseTexture", 2);
	prog.setUniform("Textures.mixNormalTexture", 3);

	model = mat4(1.0f);
	model = glm::scale(model, vec3(0.4f, 1.0f, 0.6f));
	setMatrices();
	plane.render();

	// WALLS
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, wallDiffuseTexture);

	glActiveTexture(GL_TEXTURE1);
	glBindTexture(GL_TEXTURE_2D, wallNormalTexture);

	prog.setUniform("useMixTexture", false);
	prog.setUniform("Textures.diffuseTexture", 0);
	prog.setUniform("Textures.normalTexture", 1);

	// Back Wall
	model = mat4(1.0f);
	model = glm::translate(model, vec3(0.0f, 4.0f, -15.0f));
	model = glm::rotate(model, glm::radians(90.0f), vec3(1, 0, 0));
	model = glm::scale(model, vec3(0.4f, 1.0f, 0.16f));
	setMatrices();
	plane.render();

	// Front Wall
	model = mat4(1.0f);
	model = glm::translate(model, vec3(0.0f, 4.0f, 15.0f));
	model = glm::rotate(model, glm::radians(-90.0f), vec3(1, 0, 0));
	model = glm::scale(model, vec3(0.4f, 1.0f, 0.16f));
	setMatrices();
	plane.render();

	// Left Wall
	model = mat4(1.0f);
	model = glm::translate(model, vec3(-10.0f, 4.0f, 0.0f));
	model = glm::rotate(model, glm::radians(90.0f), vec3(0, 0, 1));
	model = glm::rotate(model, glm::radians(90.0f), vec3(0, 1, 0));// Flip to face inward
	model = glm::scale(model, vec3(0.6f, 1.0f, 0.16f));
	setMatrices();
	plane.render();

	// Right wall
	model = mat4(1.0f);
	model = glm::translate(model, vec3(10.0f, 4.0f, 0.0f));
	model = glm::rotate(model, glm::radians(-90.0f), vec3(0, 0, 1));
	model = glm::rotate(model, glm::radians(-90.0f), vec3(0, 1, 0));
	model = glm::scale(model, vec3(0.6f, 1.0f, 0.16f));
	setMatrices();
	plane.render();

	prog.setUniform("showShadows", false); // Disable shadows for complex objects)

	// ROOF
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, wallDiffuseTexture);
	glActiveTexture(GL_TEXTURE1);
	glBindTexture(GL_TEXTURE_2D, wallNormalTexture);

	prog.setUniform("Textures.diffuseTexture", 0);
	prog.setUniform("Textures.normalTexture", 1);


	model = mat4(1.0f);
	model = glm::translate(model, vec3(6.0f, 8.0f, 0.0f));
	model = glm::rotate(model, glm::radians(83.0f), vec3(1, 0, 0));
	model = glm::rotate(model, glm::radians(3.0f), vec3(0, 1, 0));
	model = glm::scale(model, vec3(0.6f, 1.0f, 0.3f));
	setMatrices();
	roof->render();


	model = mat4(1.0f);
	model = glm::translate(model, vec3(-12.0f, 7.5f, 0.0f));   // horizontal mirror (X)
	model = glm::rotate(model, glm::radians(180.0f), vec3(1, 0, 0));
	model = glm::rotate(model, glm::radians(83.0f), vec3(1, 0, 0)); // vertical flip
	model = glm::rotate(model, glm::radians(3.0f), vec3(0, 1, 0));  // mirror tilt

	model = glm::scale(model, vec3(0.6f, 1.0f, 0.3f));
	setMatrices();
	roof->render();

	model = mat4(1.0f);
	model = glm::translate(model, vec3(-3.0f, 7.5f, 10.0f));   // horizontal mirror (X)
	model = glm::rotate(model, glm::radians(180.0f), vec3(1, 0, 0));
	model = glm::rotate(model, glm::radians(90.0f), vec3(0, 1, 0));
	model = glm::rotate(model, glm::radians(83.0f), vec3(1, 0, 0)); // vertical flip
	model = glm::rotate(model, glm::radians(3.0f), vec3(0, 1, 0));  // mirror tilt

	model = glm::scale(model, vec3(0.6f, 1.0f, 0.3f));
	setMatrices();
	roof->render();

	// BARRIER
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, barrierDiffuseTexture);
	glActiveTexture(GL_TEXTURE1);
	glBindTexture(GL_TEXTURE_2D, barrierNormalTexture);
	prog.setUniform("Textures.diffuseTexture", 0);
	prog.setUniform("Textures.normalTexture", 1);

	model = mat4(1.0f);
	model = glm::translate(model, vec3(-1.0f, 1.3f, 2.0f));
	model = glm::scale(model, vec3(2.5f));
	model = glm::rotate(model, glm::radians(15.0f), vec3(0, 1, 0));
	setMatrices();

	barrier->render();

	// BARREL
	//prog.setUniform("Lights[2].L", vec3(fireIntensity)); // Update fire light intensity
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, barrelDiffuseTexture);
	glActiveTexture(GL_TEXTURE1);
	glBindTexture(GL_TEXTURE_2D, barrelNormalTexture);

	prog.setUniform("Textures.diffuseTexture", 0);
	prog.setUniform("Textures.normalTexture", 1);
	model = mat4(1.0f);
	model = glm::translate(model, vec3(0.0f, 1.3f, 4.0f));
	model = glm::scale(model, vec3(3.0f));

	setMatrices();
	barrel->render();
}

// Combine HDR and bloom + tone mapping
void SceneBasic_Uniform::pass5()
{
	glDisable(GL_DEPTH_TEST);
	glDisable(GL_CULL_FACE);
	prog.setUniform("Pass", 5);
	glBindFramebuffer(GL_FRAMEBUFFER, 0);

	glClear(GL_COLOR_BUFFER_BIT);
	glViewport(0, 0, width, height);

	// Bind HDR texture to unit 0
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, hdrTexture);

	// Bind blurred bloom texture to unit 2
	glActiveTexture(GL_TEXTURE1);
	glBindTexture(GL_TEXTURE_2D, tex1);   // final blurred result

	glBindSampler(0, linearSampler);
	glBindSampler(1, linearSampler);

	glBindVertexArray(quad);
	glDrawArrays(GL_TRIANGLES, 0, 6);

	glBindSampler(1, nearestSampler);
	glEnable(GL_DEPTH_TEST);
	glEnable(GL_CULL_FACE);
}

void SceneBasic_Uniform::drawParticles() {
	// Update particles
	particleProg.use();
	particleProg.setUniform("Time", tPrev);
	particleProg.setUniform("DeltaTime", deltaTime);
	particleProg.setUniform("Pass", 1);

	glEnable(GL_RASTERIZER_DISCARD);

	glBindTransformFeedback(GL_TRANSFORM_FEEDBACK, feedback[drawBuffer]);
	glBeginTransformFeedback(GL_POINTS);

	glBindVertexArray(particleArray[1 - drawBuffer]);
	glVertexAttribDivisor(0, 0);
	glVertexAttribDivisor(1, 0);
	glVertexAttribDivisor(2, 0);
	glDrawArrays(GL_POINTS, 0, numberOfParticles);

	glBindVertexArray(0);
	glEndTransformFeedback();

	glDisable(GL_RASTERIZER_DISCARD);

	// Draw particles
	particleProg.setUniform("Pass", 2);

	// Set matrices
	particleProg.setUniform("ProjectionMatrix", projection);
	particleProg.setUniform("ModelViewMatrix", view);

	glActiveTexture(GL_TEXTURE4);
	glBindTexture(GL_TEXTURE_2D, particleTexture);

	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE);

	glDepthMask(GL_FALSE);

	glBindVertexArray(particleArray[drawBuffer]);

	glVertexAttribDivisor(0, 1);
	glVertexAttribDivisor(1, 1);
	glVertexAttribDivisor(2, 1);

	glDrawArraysInstanced(GL_TRIANGLES, 0, 6, numberOfParticles);

	glBindVertexArray(0);

	glDepthMask(GL_TRUE);
	glDisable(GL_BLEND);

	drawBuffer = 1 - drawBuffer;
	prog.use(); 

}

void SceneBasic_Uniform::resize(int w, int h)
{
	width = w;
	height = h;
	glViewport(0, 0, w, h);
}

void SceneBasic_Uniform::setMatrices()
{
	mat4 mv = view * model;
	prog.setUniform("ModelViewMatrix", mv);
	prog.setUniform("NormalMatrix", mat3(vec3(mv[0]), vec3(mv[1]), vec3(mv[2])));
	prog.setUniform("MVP", projection * mv);
	prog.setUniform("LightProjectionMatrix", shadowPV * model);



}

// Camera movement
void SceneBasic_Uniform::userInput(GLFWwindow* WindowIn)
{
	if (glfwGetKey(WindowIn, GLFW_KEY_W) == GLFW_PRESS) {
		cameraPos += cameraSpeed * deltaTime * cameraTarget; // Move forward

	}
	if (glfwGetKey(WindowIn, GLFW_KEY_S) == GLFW_PRESS) {
		cameraPos -= cameraSpeed * deltaTime * cameraTarget; // Move backward

	}
	if (glfwGetKey(WindowIn, GLFW_KEY_A) == GLFW_PRESS) {
		cameraPos -= glm::normalize(glm::cross(cameraTarget, cameraUp)) * cameraSpeed * deltaTime; // Move left

	}
	if (glfwGetKey(WindowIn, GLFW_KEY_D) == GLFW_PRESS) {
		cameraPos += glm::normalize(glm::cross(cameraTarget, cameraUp)) * cameraSpeed * deltaTime; // Move right

	}

	// Handle mouse input for camera rotation
	float sensitivity = 0.1f;

	double mouseX, mouseY;
	glfwGetCursorPos(WindowIn, &mouseX, &mouseY);

	if (firstMoved) {
		cameraLastX = mouseX;
		cameraLastY = mouseY;
		firstMoved = false;
	}

	float xOffset = mouseX - cameraLastX;
	float yOffset = cameraLastY - mouseY;

	cameraLastX = mouseX;
	cameraLastY = mouseY;

	cameraYaw += xOffset * sensitivity;
	cameraPitch += yOffset * sensitivity;

	// constrain pithc
	if (cameraPitch > 89.0f)
		cameraPitch = 89.0f;
	if (cameraPitch < -89.0f)
		cameraPitch = -89.0f;

	// Update camera
	vec3 front;
	front.x = cos(glm::radians(cameraYaw)) * cos(glm::radians(cameraPitch));
	front.y = sin(glm::radians(cameraPitch));
	front.z = sin(glm::radians(cameraYaw)) * cos(glm::radians(cameraPitch));
	cameraTarget = glm::normalize(front);

	view = glm::lookAt(cameraPos, cameraPos + cameraTarget, cameraUp);
}

// Setup framebuffer
void SceneBasic_Uniform::setupFBO()
{
	// Frame buffer
	glGenFramebuffers(1, &hdrFBO);
	glBindFramebuffer(GL_FRAMEBUFFER, hdrFBO);

	glActiveTexture(GL_TEXTURE0);
	glGenTextures(1, &hdrTexture);
	glBindTexture(GL_TEXTURE_2D, hdrTexture);

	glTexStorage2D(GL_TEXTURE_2D, 1, GL_RGB32F, width, height);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, hdrTexture, 0);

	// Depth buffer
	GLuint depthBuf;
	glGenRenderbuffers(1, &depthBuf);
	glBindRenderbuffer(GL_RENDERBUFFER, depthBuf);

	glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT, width, height);
	glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, depthBuf);

	GLenum drawBuffers[] = { GL_COLOR_ATTACHMENT0 };
	glDrawBuffers(1, drawBuffers);

	// Bloom buffer
	glGenFramebuffers(1, &blurFBO);
	glBindFramebuffer(GL_FRAMEBUFFER, blurFBO);

	bloomBufferWidth = width / 8;
	bloomBufferHeight = height / 8;

	glGenTextures(1, &tex1);
	glActiveTexture(GL_TEXTURE1);
	glBindTexture(GL_TEXTURE_2D, tex1);
	glTexStorage2D(GL_TEXTURE_2D, 1, GL_RGB32F, bloomBufferWidth, bloomBufferHeight);
	glActiveTexture(GL_TEXTURE2);
	glGenTextures(1, &tex2);
	glBindTexture(GL_TEXTURE_2D, tex2);
	glTexStorage2D(GL_TEXTURE_2D, 1, GL_RGB32F, bloomBufferWidth, bloomBufferHeight);
	// Bind tex1 to the FBO
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, tex1, 0);

	glDrawBuffers(1, drawBuffers);

	glBindFramebuffer(GL_FRAMEBUFFER, 0);

	// Shadow buffer
	glGenTextures(1, &shadowDepthTex);
	glBindTexture(GL_TEXTURE_2D, shadowDepthTex);
	glTexStorage2D(GL_TEXTURE_2D, 1, GL_DEPTH_COMPONENT32F, 2048,2048);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
	GLfloat borderColor[] = { 1.0f, 1.0f, 1.0f, 1.0f };
	glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, borderColor);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_MODE, GL_COMPARE_REF_TO_TEXTURE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_FUNC, GL_LESS);

	glGenFramebuffers(1, &shadowFBO);
	glBindFramebuffer(GL_FRAMEBUFFER, shadowFBO);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, shadowDepthTex, 0);
	glDrawBuffer(GL_NONE);
	glReadBuffer(GL_NONE);
	glBindFramebuffer(GL_FRAMEBUFFER, 0);


}

void SceneBasic_Uniform::computeLogAveLuminance()
{
	int size = width * height;
	std::vector<GLfloat>texData(size * 3);
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, hdrTexture);
	glGetTexImage(GL_TEXTURE_2D, 0, GL_RGB, GL_FLOAT, texData.data());
	float sum = 0.0f;
	for (int i = 0; i < size; ++i) {
		float lum = glm::dot(vec3(texData[i * 3], texData[i * 3 + 1], texData[i * 3 + 2]), vec3(0.2126f, 0.7152f, 0.0722f));
		sum += logf(0.00001f + lum);
	}

	prog.setUniform("AvgLum", expf(sum / size));
}

float SceneBasic_Uniform::gauss(float x, float sigma2)
{
	double coeff = 1.0 / sqrt(2.0 * glm::pi<double>() * sigma2);
	double exponent = -(x * x) / (2.0 * sigma2);
	return (float)(coeff * exp(exponent));
}

// Particle system setup
void SceneBasic_Uniform::initParticleBuffers()
{
	glGenBuffers(2, posBuffer);
	glGenBuffers(2, velBuffer);
	glGenBuffers(2, age);
	int size = numberOfParticles * 3 * sizeof(GLfloat);
	glBindBuffer(GL_ARRAY_BUFFER, posBuffer[0]);
	glBufferData(GL_ARRAY_BUFFER, size, 0, GL_DYNAMIC_COPY);
	glBindBuffer(GL_ARRAY_BUFFER, posBuffer[1]);
	glBufferData(GL_ARRAY_BUFFER, size, 0, GL_DYNAMIC_COPY);
	glBindBuffer(GL_ARRAY_BUFFER, velBuffer[0]);
	glBufferData(GL_ARRAY_BUFFER, size, 0, GL_DYNAMIC_COPY);
	glBindBuffer(GL_ARRAY_BUFFER, velBuffer[1]);
	glBufferData(GL_ARRAY_BUFFER, size, 0, GL_DYNAMIC_COPY);
	glBindBuffer(GL_ARRAY_BUFFER, age[0]);
	glBufferData(GL_ARRAY_BUFFER, numberOfParticles * sizeof(float), 0, GL_DYNAMIC_COPY);
	glBindBuffer(GL_ARRAY_BUFFER, age[1]);
	glBufferData(GL_ARRAY_BUFFER, numberOfParticles * sizeof(float), 0, GL_DYNAMIC_COPY);

	// Randomize particle spawn times
	std::vector<GLfloat> tempData(numberOfParticles);
	float rate = particleLifetime / numberOfParticles;

	for (int i = 0; i < numberOfParticles; i++) {
		tempData[i] = rate * (i - numberOfParticles);
	}
	Random::shuffle(tempData); 

	glBindBuffer(GL_ARRAY_BUFFER, age[0]);
	glBufferSubData(GL_ARRAY_BUFFER, 0, numberOfParticles * sizeof(float), tempData.data());
	glBindBuffer(GL_ARRAY_BUFFER, 0);



	// Particle array 0 
	glGenVertexArrays(2, particleArray);
	glBindVertexArray(particleArray[0]);
	glBindBuffer(GL_ARRAY_BUFFER, posBuffer[0]);
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, 0);
	glEnableVertexAttribArray(0);
	glBindBuffer(GL_ARRAY_BUFFER, velBuffer[0]);
	glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 0, 0);
	glEnableVertexAttribArray(1);
	glBindBuffer(GL_ARRAY_BUFFER, age[0]);
	glVertexAttribPointer(2, 1, GL_FLOAT, GL_FALSE, 0, 0);
	glEnableVertexAttribArray(2);

	// Particle array 1
	glBindVertexArray(particleArray[1]);
	glBindBuffer(GL_ARRAY_BUFFER, posBuffer[1]);
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, 0);
	glEnableVertexAttribArray(0);
	glBindBuffer(GL_ARRAY_BUFFER, velBuffer[1]);
	glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 0, 0);
	glEnableVertexAttribArray(1);
	glBindBuffer(GL_ARRAY_BUFFER, age[1]);
	glVertexAttribPointer(2, 1, GL_FLOAT, GL_FALSE, 0, 0);
	glEnableVertexAttribArray(2);

	glBindVertexArray(0);

	glGenTransformFeedbacks(2, feedback);
	glBindTransformFeedback(GL_TRANSFORM_FEEDBACK, feedback[0]);
	glBindBufferBase(GL_TRANSFORM_FEEDBACK_BUFFER, 0, posBuffer[0]);
	glBindBufferBase(GL_TRANSFORM_FEEDBACK_BUFFER, 1, velBuffer[0]);
	glBindBufferBase(GL_TRANSFORM_FEEDBACK_BUFFER, 2, age[0]);
	glBindTransformFeedback(GL_TRANSFORM_FEEDBACK, feedback[1]);
	glBindBufferBase(GL_TRANSFORM_FEEDBACK_BUFFER, 0, posBuffer[1]);
	glBindBufferBase(GL_TRANSFORM_FEEDBACK_BUFFER, 1, velBuffer[1]);
	glBindBufferBase(GL_TRANSFORM_FEEDBACK_BUFFER, 2, age[1]);
	glBindTransformFeedback(GL_TRANSFORM_FEEDBACK, 0);
}
