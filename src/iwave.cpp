#include "iwave.h"

#include <stdlib.h> // for calloc/free
#include <math.h>
#include <algorithm>

// NOTE: i'm lazy lol
#define DOALLOC static_cast<float*>(malloc(bufferSize))
#define DOFREE(x) free(x); x = nullptr;
#define SETZERO(x) memset(x, 0, bufferSize)

IWaveSurface::IWaveSurface(int w, int h, int p) {
	width = w;
	height = h;
	bufferCount = width * height;
	bufferSize = sizeof(float) * bufferCount;

	// CFL condition says that https://en.wikipedia.org/wiki/Courant%E2%80%93Friedrichs%E2%80%93Lewy_condition 
	// accelerationTerm <= (0.5/delta)^2 and velocityDamping <= 2/delta

	// so for 30fps:
	// accelerationTerm <= 225 and velocityDamping <= 60
	accelerationTerm = 10.0f;
	velocityDamping = 0.76f;

	// allocate grid memory
	currentGrid = DOALLOC;
	prevGrid = DOALLOC;
	verticalDerivative = DOALLOC;
	source = DOALLOC;
	obstruction = DOALLOC;

	// allocate and compute derivative kernel
	kernelRadius = p;
	kernelLength = (2 * p) + 1;
	derivativeKernel = static_cast<float*>(calloc(1, sizeof(float) * kernelLength * kernelLength));

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
			float k = static_cast<float>(x - kernelRadius);
			float l = static_cast<float>(y - kernelRadius);

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

	reset();
}

IWaveSurface::~IWaveSurface() {
	DOFREE(currentGrid);
	DOFREE(prevGrid);
	DOFREE(verticalDerivative);
	DOFREE(source);
	DOFREE(obstruction);
	DOFREE(derivativeKernel);
}

void IWaveSurface::place_source(int x, int y, float newValue) {
	int idx = get_idx(x, y);
	if (idx < 0) return;

	source[idx] = newValue;
}

// sets new values based on the minimum
void IWaveSurface::set_obstruction(int x, int y, float newValue) {
	int idx = get_idx(x, y);
	if (idx < 0) return;

	// take min of newValue and current obstruction value
	if (newValue < obstruction[idx])
		obstruction[idx] = newValue;
}

float IWaveSurface::get_height(int x, int y) {
	int idx = get_idx(x, y);
	if (idx < 0) return 0.5f;

	return currentGrid[idx];
}

float IWaveSurface::get_obstruction(int x, int y) {
	int idx = get_idx(x, y);
	if (idx < 0) return 1.0f;

	return obstruction[idx];
}

void IWaveSurface::reset() {
	SETZERO(currentGrid);
	SETZERO(prevGrid);
	SETZERO(verticalDerivative);
	SETZERO(source);

	std::fill_n(obstruction, bufferCount, 1.0f);
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

void IWaveSurface::sim_frame(float delta) {
	// preprocess sources/obstructions
	for (int y = 0; y < height; y++) {
		for (int x = 0; x < width; x++) {
			int idx = get_idx(x, y);
			//if (fabsf(currentGrid[idx]) > 100000000.0f)
			//	currentGrid[idx] /= fabsf(currentGrid[idx]);

			// apply source & obstructions
			currentGrid[idx] += source[idx];
			currentGrid[idx] *= obstruction[idx];

			// decay source
			source[idx] = 0.0f;
			//source[idx] = move_towards(source[idx], 0.0f, delta);
		}
	}

	// convolve grid with kernel, put it into verticalDerivative
	for (int y = 0; y < height; y++) {
		for (int x = 0; x < width; x++) {

			float sum = 0.0f;

			for (int dy = 0; dy < kernelLength; dy++) {
				for (int dx = 0; dx < kernelLength; dx++) {
					float kernelValue = derivativeKernel[dx + (dy * kernelLength)];

					int nx = x + dx - kernelRadius;
					int ny = y + dy - kernelRadius;

					// handle boundaries by reflecting
					if (nx < 0)
						nx = -nx;
					else if (nx >= width)
						nx = (2 * width) - nx - 1;

					if (ny < 0)
						ny = -ny;
					else if (ny >= height)
						ny = (2 * height) - ny - 1;

					// convolving with currentGrid
					sum += currentGrid[get_idx(nx, ny)] * kernelValue;
				}
			}

			verticalDerivative[get_idx(x, y)] = sum;
		}
	}

	// apply propagation - this is pretty much copied from tessendorf's "Wave Propagation" section
	float alphaDt = velocityDamping * delta;
	float onePlusAlphaDt = 1.0f + alphaDt;
	for (int i = 0; i < bufferCount; i++) {
		float temp = currentGrid[i];

		currentGrid[i] = (currentGrid[i] * (2.0f - alphaDt) / onePlusAlphaDt)
			- (prevGrid[i] / onePlusAlphaDt)
			- (verticalDerivative[i] * accelerationTerm * delta * delta / onePlusAlphaDt);

		prevGrid[i] = temp;
	}
}