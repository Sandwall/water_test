#include "iwave_gpu.h"

#include <GL/gl3w.h>
#include "external/imgui.h"

#include <stdlib.h> // for calloc/free
#include <math.h>
#include <algorithm>

// NOTE: i'm lazy lol
#define DOALLOC static_cast<float*>(malloc(bufferSize))
#define DOFREE(x) free(x); x = nullptr;
#define SETZERO(x) memset(x, 0, bufferSize)

// meant for drawing circle and rectangle shapes to the sourceObstruct textures
const char* drawAuxFragSource = /* fragment shader */ R"(
#version 460
in vec2 fragUv;
in vec2 screenUv;

out vec4 outColor;

uniform sampler2D sourceObstruct;
uniform vec2 maxValue;

void main() {
	outColor = texture(sourceObstruct, screenUv);
	float dist = max(0.0, 1.0 - length((fragUv - 0.5) * 2.0));

	// replace sources
	if(maxValue.x < 0.0) {
		outColor.x = -maxValue.x;
	} else if(maxValue.x > 0.0) {
		outColor.x += maxValue.x * dist;
		outColor.x = clamp(outColor.x, -maxValue.x, maxValue.x);
	}


	// replace obstructions
	//if(maxValue.y < 0.0) {
	//	outColor.y = 1.0 - maxValue.y;
	//	outColor.a = 1.0;
	//} else {
	//	outColor.y = dist * maxValue.y;
	//	outColor.a = dist;
	//}
}
)";

// progresses the source obstruct texture (fades sources towards zero and leaves obstructions unchanged)
const char* progressSoFragSource = /* fragment shader */ R"(
#version 460
in vec2 fragUv;
in vec2 screenUv;

out vec4 nextValue;

uniform sampler2D sourceObstruct;
uniform float speed;

float to_zero(float x, float s) {
	x -= sign(x) * s;

	if(abs(x) < s)
		x = 0.0;

	return x;
}

void main() {
	nextValue = texture(sourceObstruct, fragUv);
	nextValue.r = to_zero(nextValue.r, speed);
}
)";


const char* preprocessFragSource = /* fragment shader */ R"(
#version 460
in vec2 fragUv;
in vec2 screenUv;

out vec4 nextValue;

uniform sampler2D currentGrid;
uniform sampler2D sourceObstruct;

void main() {
	float currentValue = texture(currentGrid, fragUv).r;
	vec2 soValue = texture(sourceObstruct, fragUv).xy;

	currentValue += soValue.x;
	currentValue *= soValue.y;

	// don't care about the other 3 components...
	nextValue.x = currentValue;
}
)";

const char* convolutionFragSource = /* fragment shader */ R"(
#version 460
in vec2 fragUv;
in vec2 screenUv;

out vec4 nextValue;

uniform sampler2D currentGrid;
uniform sampler2D kernel;
uniform vec2 gridCellSize;
uniform int kernelRadius;

// this function will take much longer the farther out the requested coordinates are
// we shouldn't really run into issues with it because the kernel is a constant size
// which means that any reqested coordinates have a constant bound on how fara out they can bee
vec2 reflect_uv(vec2 uv) {
	while(uv.x < 0.0 && uv.x > 1.0) {
		uv.x = abs(uv.x);
		if(uv.x > 1.0)
			uv.x = 1.0 - uv.x;
	}

	while(uv.y < 0.0 && uv.y > 1.0) {
		uv.y = abs(uv.y);
		if(uv.y > 1.0)
			uv.y = 1.0 - uv.y;
	}

	return uv;
}

void main() {
	float sum = 0.0f;

	for(int y = -kernelRadius; y <= kernelRadius; y++) {
		for(int x = -kernelRadius; x <= kernelRadius; x++) {
			vec2 offsetUv = vec2(kernelRadius, kernelRadius) * gridCellSize;
			float kernelValue = texture(kernel, offsetUv + 0.5).r;

			offsetUv = reflect_uv(fragUv + offsetUv);
			sum += kernelValue + texture(currentGrid, offsetUv).r;
		}
	}

	// like preprocess, we don't care about .yzw
	nextValue.x = sum;
}
)";

const char* propagateFragSource = /* fragment shader */ R"(
#version 460
in vec2 fragUv;
in vec2 screenUv;

out vec4 nextValue;

uniform sampler2D currentGrid;
uniform sampler2D prevGrid;
uniform sampler2D verticalDerivative;
uniform vec3 coefficients; // these are computed once on the CPU each frame

void main() {
	float currentValue = texture(currentGrid, fragUv).r;
	float prevValue = texture(prevGrid, fragUv).r;
	float derivativeValue = texture(verticalDerivative, fragUv).r;

	nextValue.x = currentValue * coefficients.x
		+ prevValue * coefficients.y
		+ derivativeValue * coefficients.z;
}

)";

IWaveSurfaceGPU::IWaveSurfaceGPU(int w, int h, int p) {
	width = w;
	height = h;

	// CFL condition says that https://en.wikipedia.org/wiki/Courant%E2%80%93Friedrichs%E2%80%93Lewy_condition 
	// accelerationTerm <= (0.5/delta)^2 and velocityDamping <= 2/delta

	// so for 30fps: accelerationTerm <= 225 and velocityDamping <= 60
	// and for 60fps: accelerationTerm <= 899 and velocityDamping <= 120
	accelerationTerm = 20.0f;
	velocityDamping = 1.0f;

	// allocate and compute derivative kernel
	kernelRadius = p;
	kernelTexture = compute_kernel(p);

	// TODO: there are errors with creating the framebuffers... fix
	currentGrid.init(width, height);
	prevGrid.init(width, height);
	pingpongGrid.init(width, height);
	verticalDerivative.init(width, height);
	sourceObstruct.init(width, height, GL_RGBA32F);
	pingpongSO.init(width, height, GL_RGBA32F);

	drawAuxShader = Renderer::compile_shader(Renderer::vertexSource, drawAuxFragSource);
	n1_sourceObstruct = Renderer::shader_loc(drawAuxShader, "sourceObstruct");
	n1_mvp = Renderer::shader_loc(drawAuxShader, "mvp");
	n1_maxValue = Renderer::shader_loc(drawAuxShader, "maxValue");

	progressSoShader = Renderer::compile_shader(Renderer::vertexSource, progressSoFragSource);
	n2_sourceObstruct = Renderer::shader_loc(progressSoShader, "sourceObstruct");
	n2_speed = Renderer::shader_loc(progressSoShader, "speed");
	
	preprocessShader = Renderer::compile_shader(Renderer::vertexSource, preprocessFragSource);
	p1_currentGrid = Renderer::shader_loc(preprocessShader, "currentGrid");
	p1_sourceObstruct = Renderer::shader_loc(preprocessShader, "sourceObstruct");
	
	convolutionShader = Renderer::compile_shader(Renderer::vertexSource, convolutionFragSource);
	p2_currentGrid = Renderer::shader_loc(convolutionShader, "currentGrid");
	p2_kernel = Renderer::shader_loc(convolutionShader, "kernel");
	p2_gridCellSize = Renderer::shader_loc(convolutionShader, "gridCellSize");
	p2_kernelRadius = Renderer::shader_loc(convolutionShader, "kernelRadius");
	
	propagateShader = Renderer::compile_shader(Renderer::vertexSource, propagateFragSource);
	p3_currentGrid = Renderer::shader_loc(propagateShader, "currentGrid");
	p3_prevGrid = Renderer::shader_loc(propagateShader, "prevGrid");
	p3_verticalDerivative = Renderer::shader_loc(propagateShader, "verticalDerivative");
	p3_coefficients = Renderer::shader_loc(propagateShader, "coefficients");

	reset();
}

unsigned int IWaveSurfaceGPU::compute_kernel(int radius) {
	int kernelLength = (2 * radius) + 1;
	float* derivativeKernel = static_cast<float*>(calloc(1, sizeof(float) * kernelLength * kernelLength));

	if (!derivativeKernel) return { 0 };

	// G0 scales the kernel so that the center value is 1.0f
	float G0 = 0.0f;

	// these are parameters for calculating G0 and the kernel
	int n = 10000;
	float dq = 0.001f; // this is apparently a good choice for accuracy
	float sigma = 1.0f; // this is some sigma that makes the sum converge to a reasonable number
	// but the symbol isn't rendering in the PDF so idk what it represents

	// calculate G0
	for (int i = 1; i <= n; i++) {
		float qi2 = (dq * static_cast<float>(i)) * (dq * static_cast<float>(i));
		G0 += qi2 * expf(-sigma * qi2);
	}

	// now compute the derivative kernel G(k, l)
	for (int y = 0; y < kernelLength; y++) {
		for (int x = 0; x < kernelLength; x++) {
			int idx = x + (y * kernelLength);
			float k = static_cast<float>(x - radius);
			float l = static_cast<float>(y - radius);

			float r = sqrtf((k * k) + (l * l));

			for (int i = 1; i <= n; i++) {
				float qi = dq * static_cast<float>(i);
				float qi2 = qi * qi;

				derivativeKernel[idx] += qi2 * expf(-sigma * qi2) * static_cast<float>(_j0(qi * r)) / G0;
			}
		}
	}

	for (int y = 0; y < kernelLength; y++) {
		for (int x = 0; x < kernelLength; x++) {
			int idx = x + (y * kernelLength);
			printf("%f \t", derivativeKernel[idx]);
		}
		printf("\n");
	}

	unsigned int derivativeTexture = Renderer::create_tex(kernelLength, kernelLength, GL_R32F, derivativeKernel);
	free(derivativeKernel);
	return derivativeTexture;
}

IWaveSurfaceGPU::~IWaveSurfaceGPU() {
}

// this might be a bit expensive since it copies a super large texture for each call
// but we can disregard the performance of it since in a game scenario we would just build the texture from scratch each frame
void IWaveSurfaceGPU::place_source(int x, int y, float r, float strength) {
	pingpongSO.copy_from(sourceObstruct);
	sourceObstruct.set_target();

	glUseProgram(drawAuxShader);

	glDisable(GL_DEPTH_TEST);
	glDisable(GL_BLEND);
	glDisable(GL_CULL_FACE);

	Renderer::bind_tex(0, pingpongSO.texture);
	Renderer::uniform_tex(drawAuxShader, 0, n1_sourceObstruct);
	glUniform2f(n1_maxValue, strength, 0.0f);

	Renderer::sampler_settings();
	Renderer::draw_transformed_quad(static_cast<float>(x), static_cast<float>(y), r, r);
	Renderer::bind_tex(0, 0);

	glEnable(GL_CULL_FACE);
	glEnable(GL_BLEND);
	glEnable(GL_DEPTH_TEST);

	glUseProgram(0);
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

// sets new values based on the minimum
void IWaveSurfaceGPU::set_obstruction(int x, int y, float r, float strength) {
	// copied straight from place_source and tweaked
}

void IWaveSurfaceGPU::reset() {
	glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
	
	currentGrid.set_target();
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	prevGrid.set_target();
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	pingpongGrid.set_target();
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	verticalDerivative.set_target();
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	glClearColor(0.0f, 1.0f, 0.0f, 1.0f);

	sourceObstruct.set_target();
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	pingpongSO.set_target();
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void IWaveSurfaceGPU::sim_frame(float delta) {
	// our steps are as follows:
	// - render to pingpong, use sourceObstruct as input
	// - render to verticalDerivative, use pingpong as input
	// - render to currentGrid, use previousGrid as input, and read from current value of pingpong
	// - render pingpong to previous
	// - swap pingpong and current

	//const Matrix identity = matrixIdentity();

	//
	// preprocess sources / obstructions
	//

	//copy_tex(currentGrid, pingpongGrid);

	//rlEnableFramebuffer(currentGrid.framebuffer);
	//rlEnableShader(preprocessShader.id);

	//rlActiveTextureSlot(0);
	//rlEnableTexture(pingpongGrid.texture);
	//rlSetUniformSampler(p1_currentGrid, pingpongGrid.texture);
	//rlActiveTextureSlot(1);
	//rlEnableTexture(sourceObstruct.texture);
	//rlSetUniformSampler(p1_sourceObstruct, sourceObstruct.texture);

	//draw_quad();

	//rlActiveTextureSlot(0);
	//rlDisableTexture();
	//rlActiveTextureSlot(1);
	//rlDisableTexture();

	// progress source obstruct (either fade sources towards 0 or zero them out)
	//copy_tex(sourceObstruct, pingpongSO);
	//rlEnableFramebuffer(sourceObstruct.framebuffer);
	//rlEnableShader(progressSoShader.id);
	//rlActiveTextureSlot(0);
	//rlEnableTexture(pingpongSO.texture);
	//rlSetUniformSampler(n2_sourceObstruct, pingpongSO.texture);
	//rlSetUniform(n2_speed, &delta, RL_SHADER_UNIFORM_FLOAT, 1);
	//draw_quad();
	//rlDisableTexture();

	//
	// convolve grid with kernel, put it into verticalDerivative
	//
	/*rlEnableFramebuffer(verticalDerivative.framebuffer);
	rlEnableShader(convolutionShader.id);

	rlActiveTextureSlot(0);
	rlEnableTexture(currentGrid.texture);
	rlActiveTextureSlot(1);
	rlEnableTexture(kernelTexture);
	rlSetUniformSampler(p2_currentGrid, currentGrid.texture);
	rlSetUniformSampler(p2_kernel, kernelTexture);
	float gridCellSize[2] = { 1.0f / static_cast<float>(width), 1.0f / static_cast<float>(height) };
	rlSetUniform(p2_gridCellSize, gridCellSize, RL_SHADER_UNIFORM_VEC2, 1);
	rlSetUniform(p2_kernelRadius, &kernelRadius, RL_SHADER_UNIFORM_INT, 1);

	draw_quad();

	rlActiveTextureSlot(0);
	rlDisableTexture();
	rlActiveTextureSlot(1);
	rlDisableTexture();*/

	//
	// apply propagation
	//

/*
	rlEnableFramebuffer(pingpongGrid.framebuffer);
	rlEnableShader(propagateShader.id);

	rlActiveTextureSlot(0);
	rlEnableTexture(currentGrid.texture);
	rlActiveTextureSlot(1);
	rlEnableTexture(prevGrid.texture);
	rlActiveTextureSlot(2);
	rlEnableTexture(verticalDerivative.texture);

	rlSetUniformSampler(p3_currentGrid, pingpongGrid.texture);
	rlSetUniformSampler(p3_prevGrid, prevGrid.texture);
	rlSetUniformSampler(p3_verticalDerivative, verticalDerivative.texture);

	// these mirror the coefficients of currentGrid, prevGrid, verticalDerivative in the CPU version
	float alphaDt = velocityDamping * delta;
	float coefficients[3] = {
		(2.0f - alphaDt) / (1.0f + alphaDt),
		-1.0f / (1.0f + alphaDt),
		-accelerationTerm * delta * delta / (1.0f + alphaDt)
	};
	rlSetUniform(p3_coefficients, coefficients, RL_SHADER_UNIFORM_VEC3, 1);

	draw_quad();

	rlActiveTextureSlot(0);
	rlDisableTexture();
	rlActiveTextureSlot(1);
	rlDisableTexture();
	rlActiveTextureSlot(2);
	rlDisableTexture();
*/
	//rlEnableBackfaceCulling();
	//rlEnableDepthTest();
	//rlDisableShader();
	//rlDisableFramebuffer();

	//std::swap(prevGrid, pingpongGrid);
}

GLuint IWaveSurfaceGPU::get_display() {
	Renderer::sampler_settings();
	return sourceObstruct.texture;
}

void IWaveSurfaceGPU::imgui_builder() {
	if (ImGui::Begin("IWaveSurfaceGPU")) {
		ImGui::SeparatorText("Kernel Texture");
		ImGui::Image(kernelTexture, ImVec2(128, 128));


	}
	ImGui::End();
}