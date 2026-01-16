#include "iwave_gpu.h"

#include <GL/gl3w.h>
#include <GLFW/glfw3.h>
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
#version 460 core

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
	if(maxValue.y < 0.0) {
		// maxValue.y == -1.0 means it gets set to 0
		outColor.y = 1.0 + maxValue.y;
	} else {
		//outColor.y = dist * maxValue.y;
	}
}
)";

// progresses the source obstruct texture (fades sources towards zero and leaves obstructions unchanged)
const char* progressSoFragSource = /* fragment shader */ R"(
#version 460 core

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
	nextValue.r = 0.0;
	//nextValue.r = to_zero(nextValue.r, speed);
}
)";


const char* preprocessFragSource = /* fragment shader */ R"(
#version 460 core

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
#version 460 core

in vec2 fragUv;
in vec2 screenUv;

out vec4 nextValue;

uniform sampler2D currentGrid;
uniform sampler2D kernel;
uniform vec2 kernelCellSize;
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
			vec2 offset = vec2(x, y);
			float kernelValue = 
			sum += texture(kernel, (offset * kernelCellSize) + 0.5).r
				*  texture(currentGrid, reflect_uv(fragUv + (offset * gridCellSize))).r;
		}
	}

	// like preprocess, we don't care about .yzw
	nextValue.x = sum;
}
)";

const char* propagateFragSource = /* fragment shader */ R"(
#version 460 core

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
	p2_kernelCellSize = Renderer::shader_loc(convolutionShader, "kernelCellSize");
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
// but we can disregard the performance of it since in a game scenario we would just draw the texture from scratch each frame
void IWaveSurfaceGPU::draw_aux(int x, int y, float r, float v1, float v2) {
	pingpongSO.copy_from(sourceObstruct);
	sourceObstruct.set_target();

	glUseProgram(drawAuxShader);

	glDisable(GL_DEPTH_TEST);
	glDisable(GL_BLEND);
	glDisable(GL_CULL_FACE);

	Renderer::attach_tex(drawAuxShader, n1_sourceObstruct, pingpongSO.texture, 0);
	glUniform2f(n1_maxValue, v1, v2);

	Renderer::draw_transformed_quad(static_cast<float>(x), static_cast<float>(y), r, r);
	Renderer::bind_tex(0, 0);

	glEnable(GL_CULL_FACE);
	glEnable(GL_BLEND);
	glEnable(GL_DEPTH_TEST);

	glUseProgram(0);
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void IWaveSurfaceGPU::place_source(int x, int y, float r, float strength) {
	draw_aux(x, y, r, strength, 0.0f);
}

void IWaveSurfaceGPU::set_obstruction(int x, int y, float r, float strength) {
	draw_aux(x, y, r, 0.0f, -strength);
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
	glDisable(GL_DEPTH_TEST);
	glDisable(GL_BLEND);
	glDisable(GL_CULL_FACE);

	// our steps are as follows:
	// - render to pingpong, use sourceObstruct as input
	// - render to verticalDerivative, use pingpong as input
	// - render to currentGrid, use previousGrid as input, and read from current value of pingpong
	// - render pingpong to previous
	// - swap pingpong and current

	//
	// preprocess sources / obstructions
	//

	pingpongGrid.copy_from(currentGrid);
	currentGrid.set_target();
	glUseProgram(preprocessShader);

	Renderer::attach_tex(preprocessShader, p1_currentGrid, pingpongGrid.texture, 0);
	Renderer::attach_tex(preprocessShader, p1_sourceObstruct, sourceObstruct.texture, 1);
	Renderer::draw_quad();

	// progress source obstruct (either fade sources towards 0 or zero them out)
	pingpongSO.copy_from(sourceObstruct);
	sourceObstruct.set_target();
	glUseProgram(progressSoShader);

	Renderer::attach_tex(progressSoShader, n2_sourceObstruct, pingpongSO.texture, 0);
	glUniform1f(n2_speed, delta);
	Renderer::draw_quad();


	//
	// convolve grid with kernel, put it into verticalDerivative
	//
	verticalDerivative.set_target();
	glUseProgram(convolutionShader);

	Renderer::attach_tex(convolutionShader, p2_currentGrid, currentGrid.texture, 0);
	Renderer::attach_tex(convolutionShader, p2_kernel, kernelTexture, 1);
	glUniform2f(p2_gridCellSize, 1.0f / static_cast<float>(width), 1.0f / static_cast<float>(height));
	glUniform2f(p2_kernelCellSize, 1.0f / static_cast<float>((kernelRadius * 2) + 1), 1.0f / static_cast<float>((kernelRadius * 2) + 1));
	glUniform1i(p2_kernelRadius, kernelRadius);
	Renderer::draw_quad();

	//
	// apply propagation
	//
	pingpongGrid.copy_from(currentGrid);
	currentGrid.set_target();
	glUseProgram(propagateShader);
	
	Renderer::attach_tex(propagateShader, p3_currentGrid, currentGrid.texture, 0);
	Renderer::attach_tex(propagateShader, p3_prevGrid, prevGrid.texture, 1);
	Renderer::attach_tex(propagateShader, p3_verticalDerivative, verticalDerivative.texture, 2);

	// these mirror the coefficients of currentGrid, prevGrid, verticalDerivative in the CPU version
	float alphaDt = velocityDamping * delta;
	float coefficients[3] = {
		(2.0f - alphaDt) / (1.0f + alphaDt),
		-1.0f / (1.0f + alphaDt),
		-accelerationTerm * delta * delta / (1.0f + alphaDt)
	};
	glUniform3f(p3_coefficients, coefficients[0], coefficients[1], coefficients[2]);
	Renderer::draw_quad();

	prevGrid.copy_from(pingpongGrid);

	glBindFramebuffer(GL_FRAMEBUFFER, 0);

	glEnable(GL_CULL_FACE);
	glEnable(GL_BLEND);
	glEnable(GL_DEPTH_TEST);
}

GLuint IWaveSurfaceGPU::get_display() {
	return currentGrid.texture;
}

extern GLFWwindow* window;            // also from main.cpp
void IWaveSurfaceGPU::imgui_builder(bool* open) {
	if (open && *open) {
		int winWidth = 1280, winHeight = 720;
		glfwGetWindowSize(window, &width, &height);
		float aspect = static_cast<float>(width) / static_cast<float>(height);
		float imgWidth = 128.0f;
		float imgHeight = imgWidth / aspect;

		ImGui::SetNextWindowPos(ImVec2(winWidth - (2 * imgWidth), 16), ImGuiCond_Appearing);

		if (ImGui::Begin("IWaveSurfaceGPU"), open, ImGuiWindowFlags_AlwaysAutoResize) {
			ImGui::SeparatorText("Kernel Texture");
			ImGui::Image(kernelTexture, ImVec2(imgWidth, imgWidth));

			ImGui::SeparatorText("Source and Obstructions");
			ImGui::Image(sourceObstruct.texture, ImVec2(imgWidth, imgHeight));

			ImGui::SeparatorText("Vertical Derivative");
			ImGui::Image(verticalDerivative.texture, ImVec2(imgWidth, imgHeight));

			ImGui::SeparatorText("Previous Grid");
			ImGui::Image(prevGrid.texture, ImVec2(imgWidth, imgHeight));
		}

		ImGui::End();
	}
}