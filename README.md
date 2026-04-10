# Optimised Developer Tool – OpenGL Shader Project

## Development Environment

- **IDE:** Visual Studio 2022
- **Operating System:** Windows 11 (64-bit)
- **OpenGL Version:** 4.6
- **GLSL Version:** 460

---

## Overview

This project is a continuation and extension of the CW1 prototype. The same abandoned industrial scene is used as a base, but more models and three new rendering features have been added for CW2:

- Fire particle system using transform feedback
- Dynamic shadow mapping from a directional light source
- Animated procedural cloud skybox created by a 2D noise texture

The CW1 features — Blinn-Phong shading, normal mapping, texture blending, HDR rendering, Gaussian bloom, gamma correction, fog, and skybox

---

## How to Open and Run the Executable

1. Extract the `.zip` archive to a local folder.
2. Open the folder and run `COMP3015_CW2.exe` directly

### Controls

| Input | Action |
|---|---|
| `W` `A` `S` `D` | Move camera forward / left / backward / right |
| Mouse movement | Look around (yaw and pitch) |
| `Esc` | Close the application |

The camera is clamped to stay within the building boundaries. 

---

## How the Program Works

### Code Structure

All core logic lives in `SceneBasic_Uniform.cpp` and its header. The class inherits from the module template's `Scene` base and implements the standard game loop via `initScene()`, `update()`, and `render()`.

| Function | Responsibility |
|---|---|
| `initScene()` | Loads models and textures, compiles shaders, sets up all FBOs, configures sampler objects, seeds particle buffers, generates noise texture |
| `compile()` | Compiles and links the three shader programs: main scene (`basic_uniform`), skybox, and particles |
| `update()` | Advances time, processes keyboard and mouse input, updates the view matrix |
| `render()` | Renders the full multi-pass pipeline each frame update |
| `shadowPass()` | Renders a depth-only pass from the light's point of view into the shadow FBO, producing shadows |
| `pass1()` – `pass5()` | HDR scene render, bloom extraction, vertical blur, horizontal blur, tone mapping and composite |
| `drawScene()` | Draw calls for all geometry with correct textures and transforms |
| `drawParticles()` | Runs the transform feedback update pass then the render pass for fire particles |
| `setupFBO()` | Creates the HDR framebuffer, bloom framebuffer, and shadow depth framebuffer |
| `computeLogAveLuminance()` | Reads back the HDR texture each frame to compute average luminance for tone mapping |
| `userInput()` | Handles WASD movement and mouse-look, clamps camera within the scene |
| `initParticleBuffers()` | Allocates VBOs and VAOs for particle position, velocity, and age |
| `gauss()` | Computes Gaussian weights for the blur passes |

### Shader Programs

**`basic_uniform.vert` / `basic_uniform.frag`**
The main scene shader. The vertex shader transforms positions, normals, and texture coordinates, and computes the shadow coordinate by multiplying the world-space position by the light's projection-view-bias matrix. The fragment shader handles all lighting, normal mapping, texture blending, fog, shadow lookup, HDR brightness extraction, Gaussian blur (passes 3 and 4), and tone mapping (pass 5) via a `Pass` uniform that branches to the correct sub-function.

**`skybox.vert` / `skybox.frag`**
Renders the HDR cubemap. The view matrix has its translation stripped (`mat4(mat3(view))`) so the skybox stays centred on the camera. The fragment shader samples the noise texture offset by time to animate moving clouds across the sky plane.

**`particles.vert` / `particles.frag`**
A particle shader including 2 passes. Pass 1 runs the particle simulation: age is advanced, dead particles reset at the emitter with randomised position and velocity sampled from a 1D random texture, live particles integrate velocity with acceleration. Pass 2 renders particles as instanced quads by offsetting from the view-space position.

---

## Rendering Pipeline

Each frame executes in this order:

1. **Shadow pass** — depth-only render from the light's POV into a 2048×2048 shadow FBO
2. **Pass 1 (HDR)** — full scene render with lighting, normal maps, fog, shadow lookup, and particles, into a floating-point FBO
3. **Pass 2** — brightness extraction: pixels below the luminance threshold are discarded
4. **Pass 3** — vertical Gaussian blur on the bright regions
5. **Pass 4** — horizontal Gaussian blur to complete the separable filter
6. **Pass 5** — tone mapping (XYZ colour space, Reinhard-style white point compression), bloom composite, and gamma correction (2.2) to the default framebuffer

---

## What Makes the Shader Program Special

### Starting Point

The project began from the module's Visual Studio template provided at the start of the unit, including the base scene functions of `initScene()`, `update()`, and `render()`

### CW2 Feature 1 — GPU Particle System with Transform Feedback

The fire particle system keeps all simulation on the GPU using OpenGL transform feedback. Two sets of buffers (position, velocity, age) are ping-ponged each frame: one set is read as vertex attributes while the other is written as transform feedback output, then the roles swap.

The key additions beyond the base lab are a **swirl effect** and **randomised fade**. The swirl is computed in the vertex shader by taking the particle's offset from the emitter centre, rotating it 90° in the XZ plane, and feeding a fraction of that as a velocity addition each frame — giving the fire a slow rotating drift rather than purely vertical movement. The fade uses a per-particle random value sampled from the 1D random texture so each particle dims at a slightly different rate, avoiding the flat synchronised fade of a naive `1.0 - lifeRatio` approach.

Other additional features include particle accelleration, increasing velocity over the particles lifetime, and particles shrinking over their lifetime.

### CW2 Feature 2 — Shadow Mapping

Shadows are produced by a depth-only pre-pass rendered from a directional light positioned above the scene, creating the illusion of shadows created by the skybox. The depth texture is bound as a `sampler2DShadow` and sampled with `textureProj()` in the main fragment shader.

Shadows are deliberately applied only to the floor and walls. Complex meshes with thin geometry (crowbar, chair, etc) were excluded because the single-sample depth map produces sharp-edged shadows on fine features at this resolution.

### CW2 Feature 3 — Animated Noise Skybox

The skybox fragment shader samples a 2D Perlin-style noise texture generated by `NoiseTex::generate2DTex()` with `GL_MIRRORED_REPEAT` wrapping to avoid seams. A `Time` uniform scrolls the UV coordinates each frame, producing the appearance of clouds drifting across the night sky, continously looping. 

### Comparison to Similar Systems

Unity's and Unreal's particle system both handle fire effects through a
visual graph editor — the artist sets emitter shape, lifetime curves, and colour gradients
without writing a line of shader code. The engine manages the GPU buffers, update loop,
and billboarding internally.

This implementation does the same job, recreated a similar system. Transform feedback
replaces the engine's internal buffer ping-pong: particle state (position, velocity, age)
is written back to GPU memory each frame without ever touching the CPU, which is
the same approach those engines use internally. The swirl effect and randomised fade
— both written directly in GLSL — would be equivalent to a custom Velocity over
Lifetime module and a Random Between Two Constants node in Unity's graph. The
difference is that every part of the behaviour is explicit and readable in the vertex
shader rather than configured through a UI.

The shadow mapping implementation follows the standard two-pass depth map approach,
but integrating it into an existing multi-pass HDR pipeline required careful management
of FBO bindings, sampler objects, and depth buffer state across passes — something a
Unity project handles automatically through its render pipeline abstraction. The polygon
offset tuning to eliminate shadow acne is a good example of a problem Unity hides
completely (via its Shadow Bias slider) but which has to be reasoned about explicitly
in raw OpenGL.

The noise skybox is comparable to Unity's Visual Effect Graph cloud techniques or
Unreal's Sky Atmosphere system, both of which use noise functions to animate cloud
coverage. Those systems use 3D volumetric noise and raymarch through a cloud layer.
The approach here is a 2D scrolling texture — far simpler, but sufficient for a
background element in a scene where the focus is on the interior lighting.

---

## Additional Notes

**Shadow mapping vs shadow volumes:** Shadow volumes produce mathematically precise
shadows at any resolution but the cost scales with scene geometry complexity, as
every shadow-casting mesh needs its silhouette extruded each frame. Shadow mapping
fixes the cost largely independent of scene complexity, making it the more practical
choice for a scene with multiple detailed meshes. The trade-off is fixed resolution
and artefacts that shadow volumes avoid entirely, but the implementation complexity
is significantly lower.

**Animated shadows scrapped:** An early goal was to have the fire light cast a
flickering shadow in sync with the particles. This required either re-running the
shadow pass every frame from a shifting light position, which an attempted implementation
showed to look poor quality.
---

## Links

- **GitHub Repository:** https://github.com/JamHam04/COMP3015_CW2
- **Video Report:** https://youtu.be/RWzpj5qdW4w

---

## Models and Textures

| Asset | Source |
|---|---|
| Barrel, Broken Wall, Concrete Barrier, Crowbar, trashcan, chair | https://polyhaven.com |
| Textures (diffuse, normal) | https://polyhaven.com / https://cc0-textures.com |
| Fire particle texture | Lab Resource |