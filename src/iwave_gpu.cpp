#include "iwave_gpu.h"

#include <stdlib.h> // for calloc/free
#include <math.h>
#include <algorithm>

#include <rlgl.h>
#include <external/glad.h>

// NOTE: i'm lazy lol
#define DOALLOC static_cast<float*>(malloc(bufferSize))
#define DOFREE(x) free(x); x = nullptr;
#define SETZERO(x) memset(x, 0, bufferSize)

const char* defaultVertexSource = R"(
#version 330

in vec3 vertexPosition;
in vec2 vertexTexCoord;
in vec4 vertexColor;

out vec2 fragTexCoord;
out vec4 fragColor;

uniform mat4 mvp;

void main() {
	fragTexCoord = vertexTexCoord;
	fragColor = vertexColor;
	gl_Position = mvp*vec4(vertexPosition, 1.0);
}
)";

/* Here's the raylib default fragment shader for OpenGL 3.3, just for reference:

#version 330

in vec2 fragTexCoord;
in vec4 fragColor;

out vec4 finalColor;

uniform sampler2D currentGrid;
uniform vec4 colDiffuse;

void main() {
	vec4 texelColor = texture(currentGrid, fragTexCoord);
	finalColor = texelColor * colDiffuse * fragColor;
}

*/

const char* preprocessFragSource = /* fragment shader */ R"(
#version 330

in vec2 fragTexCoord;
in vec4 fragColor;

out vec4 nextValue;

uniform sampler2D currentGrid;
uniform sampler2D sourceObstruct;

void main() {
	float currentValue = texture(currentGrid, fragTexCoord).r;
	vec2 soValue = texture(sourceObstruct, fragTexCoord).xy;

	currentValue += soValue.x;
	currentValue *= soValue.y;

	// don't care about the other 3 components...
	nextValue.x = currentValue;
}
)";

const char* convolutionFragSource = /* fragment shader */ R"(
#version 330

in vec2 fragTexCoord;
in vec4 fragColor;

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

			offsetUv = reflect_uv(fragTexCoord + offsetUv);
			sum += kernelValue + texture(currentGrid, offsetUv).r;
		}
	}

	// like preprocess, we don't care about .yzw
	nextValue.x = sum;
}
)";

const char* propagateFragSource = /* fragment shader */ R"(
#version 330

in vec2 fragTexCoord;
in vec4 fragColor;

out vec4 nextValue;

uniform sampler2D currentGrid;
uniform sampler2D prevGrid;
uniform sampler2D verticalDerivative;
uniform vec3 coefficients; // these are computed once on the CPU each frame

void main() {
	float currentValue = texture(currentGrid, fragTexCoord).r;
	float prevValue = texture(prevGrid, fragTexCoord).r;
	float derivativeValue = texture(verticalDerivative, fragTexCoord).r;

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

	// load shaders & create grids on gpu
	preprocessShader = LoadShaderFromMemory(defaultVertexSource, preprocessFragSource);
	convolutionShader = LoadShaderFromMemory(defaultVertexSource, convolutionFragSource);
	propagateShader = LoadShaderFromMemory(defaultVertexSource, propagateFragSource);
	currentGrid = TextureTarget::create(width, height);
	prevGrid = TextureTarget::create(width, height);
	pingpongGrid = TextureTarget::create(width, height);
	verticalDerivative = TextureTarget::create(width, height);
	sourceObstruct = TextureTarget::create(width, height, PIXELFORMAT_UNCOMPRESSED_R32G32B32A32);

	// allocate and compute derivative kernel
	kernelRadius = p;
	kernelTexture = compute_kernel(p);

	// cache shader uniform locations
	p1_currentGrid = GetShaderLocation(preprocessShader, "currentGrid");
	p1_sourceObstruct = GetShaderLocation(preprocessShader, "sourceObstruct");

	p2_currentGrid = GetShaderLocation(convolutionShader, "currentGrid");
	p2_kernel = GetShaderLocation(convolutionShader, "kernel");
	p2_gridCellSize = GetShaderLocation(convolutionShader, "gridCellSize");
	p2_kernelRadius = GetShaderLocation(convolutionShader, "kernelRadius");

	p3_currentGrid = GetShaderLocation(propagateShader, "currentGrid");
	p3_prevGrid = GetShaderLocation(propagateShader, "prevGrid");
	p3_verticalDerivative = GetShaderLocation(propagateShader, "verticalDerivative");
	p3_coefficients = GetShaderLocation(propagateShader, "coefficients");

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

	unsigned int derivativeTexture = TextureTarget::create_only_tex(kernelLength, kernelLength, PIXELFORMAT_UNCOMPRESSED_R32, derivativeKernel);;
	free(derivativeKernel);
	return derivativeTexture;
}

IWaveSurfaceGPU::~IWaveSurfaceGPU() {
}

void IWaveSurfaceGPU::place_source(int x, int y, float r, float strength) {

}

// sets new values based on the minimum
void IWaveSurfaceGPU::set_obstruction(int x, int y, int rx, int ry, float strength) {

}

void IWaveSurfaceGPU::reset() {
	rlClearColor(0, 0, 0, 0);

	rlBindFramebuffer(RL_DRAW_FRAMEBUFFER, currentGrid.framebuffer);
	rlClearScreenBuffers();
	rlBindFramebuffer(RL_DRAW_FRAMEBUFFER, prevGrid.framebuffer);
	rlClearScreenBuffers();
	rlBindFramebuffer(RL_DRAW_FRAMEBUFFER, pingpongGrid.framebuffer);
	rlClearScreenBuffers();

	rlBindFramebuffer(RL_DRAW_FRAMEBUFFER, verticalDerivative.framebuffer);
	rlClearScreenBuffers();

	rlBindFramebuffer(RL_DRAW_FRAMEBUFFER, sourceObstruct.framebuffer);
	rlClearScreenBuffers();
}

void IWaveSurfaceGPU::sim_frame(float delta) {
	// our steps are as follows:
	// - render to pingpong, use sourceObstruct as input
	// - render to verticalDerivative, use pingpong as input
	// - render to currentGrid, use previousGrid as input, and read from current value of pingpong
	// - render pingpong to previous
	// - swap pingpong and current

	//
	// preprocess sources / obstructions
	//

	rlBindFramebuffer(RL_DRAW_FRAMEBUFFER, pingpongGrid.framebuffer);
	rlEnableShader(preprocessShader.id);
	//rlSetShader(preprocessShader.id, preprocessShader.locs);

	rlSetUniformSampler(p1_currentGrid, currentGrid.texture);
	rlSetUniformSampler(p1_sourceObstruct, sourceObstruct.texture);

	rlLoadDrawQuad();

	//
	// convolve grid with kernel, put it into verticalDerivative
	//
	rlBindFramebuffer(RL_DRAW_FRAMEBUFFER, verticalDerivative.framebuffer);
	rlEnableShader(convolutionShader.id);
	//rlSetShader(convolutionShader.id, convolutionShader.locs);

	rlSetUniformSampler(p2_currentGrid, pingpongGrid.texture);
	rlSetUniformSampler(p2_kernel, kernelTexture);
	float gridCellSize[2] = { 1.0f / static_cast<float>(width), 1.0f / static_cast<float>(height) };
	rlSetUniform(p2_gridCellSize, gridCellSize, RL_SHADER_UNIFORM_VEC2, 1);
	rlSetUniform(p2_kernelRadius, &kernelRadius, RL_SHADER_UNIFORM_INT, 1);

	rlLoadDrawQuad();

	//
	// apply propagation
	//
	
	rlBindFramebuffer(RL_DRAW_FRAMEBUFFER, currentGrid.framebuffer);
	rlEnableShader(propagateShader.id);
	//rlSetShader(propagateShader.id, propagateShader.locs);
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

	rlLoadDrawQuad();

	// requires us to compile raylib with GRAPHICS_API_OPENGL_43 #defined
	glBindTexture(GL_TEXTURE_2D, pingpongGrid.texture);
	glCopyTextureSubImage2D(prevGrid.texture, 0, 0, 0, 0, 0, width, height);
	std::swap(prevGrid, pingpongGrid);
}

Texture IWaveSurfaceGPU::get_display() {
	return {
		.id = currentGrid.texture,
		.width = width,
		.height = height,
		.mipmaps = 0,
		.format = PIXELFORMAT_UNCOMPRESSED_R32
	};
}