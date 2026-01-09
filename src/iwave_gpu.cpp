#include "iwave_gpu.h"

#include <stdlib.h> // for calloc/free
#include <math.h>
#include <algorithm>

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

/* TODO: put simulation logic here
 * 
 * I'll have to think if the simulation has to be done in 3 passes again
 * or maybe if I can condense it into fewer.
 * 
 * I also don't think Raylib supports MRT (multiple render targets) 
 * but I guess conceptually it would make writing the shaders a bit simpler
 * 
 * Then again if we just used compute shaders, choosing the 
 */

const char* simShaderFragSource = R"(
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
)";

static Texture create_tex_format(int w, int h, int format = PIXELFORMAT_UNCOMPRESSED_R32) {
	Image img = GenImageColor(w, h, WHITE);
	ImageFormat(&img, format);
	Texture tex = LoadTextureFromImage(img);
	UnloadImage(img);

	return tex;
}

IWaveSurfaceGPU::IWaveSurfaceGPU(int w, int h, int p) {
	width = w;
	height = h;

	// CFL condition says that https://en.wikipedia.org/wiki/Courant%E2%80%93Friedrichs%E2%80%93Lewy_condition 
	// accelerationTerm <= (0.5/delta)^2 and velocityDamping <= 2/delta

	// so for 30fps: accelerationTerm <= 225 and velocityDamping <= 60
	// and for 60fps: accelerationTerm <= 899 and velocityDamping <= 120
	accelerationTerm = 20.0f;
	velocityDamping = 1.0f;

	// create grids on gpu
	simShader = LoadShaderFromMemory(defaultVertexSource, simShaderFragSource);

	grids[0] = create_tex_format(width, height);
	grids[1] = create_tex_format(width, height);
	grids[2] = create_tex_format(width, height);
	sourceObstruct = create_tex_format(width, height, PIXELFORMAT_UNCOMPRESSED_R32G32B32A32);

	// allocate and compute derivative kernel
	kernelRadius = p;
	kernel = compute_kernel(p);

	reset();
}

Texture IWaveSurfaceGPU::compute_kernel(int radius) {
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

	Texture derivativeTexture = create_tex_format(kernelLength, kernelLength);
	UpdateTexture(derivativeTexture, derivativeKernel);
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

}

static float move_towards(float current, float target, float step) {
	float diff = target - current;
	if (diff > 0.0f) {
		if (diff < step)
			return target;

		return current + step;
	}
	else if (diff < 0.0f) {
		if (diff > step)
			return target;

		return current - step;
	}
	else
		return current;
}

void IWaveSurfaceGPU::sim_frame(float delta) {
	// preprocess sources/obstructions
	// want to render to grids[nextGrid], use grids[currentGrid] and sourceObstruct as input

	// convolve grid with kernel, put it into verticalDerivative
	// we want to render to verticalDerivative, use grids[nextGrid] as an input

	// apply propagation - this is pretty much copied from tessendorf's "Wave Propagation" section
	// we want to render to grids[currentGrid], use grids[nextGrid] as input (to represent the current grid)
	// and use grids[prevGrid] as input to represent the previousGrid

	// now increment currentGrid
	// this will make 
}

Texture* IWaveSurfaceGPU::get_display() {
	return &grids[currentGrid];
}