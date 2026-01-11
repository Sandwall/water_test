#pragma once

#include "surface_sim.h"
#include <raylib.h>

// https://people.computing.clemson.edu/~jtessen/reports/papers_files/Interactive_Water_Surfaces.pdf
class IWaveSurface : public SurfaceSim {
	int width = 0, height = 0;
	int bufferCount = 0;
	int bufferSize = 0;

	// for simulation
	float* currentGrid = nullptr;
	float* prevGrid = nullptr;
	float* verticalDerivative = nullptr;
	float* source = nullptr;
	float* obstruction = nullptr;

	// convolution kernel
	float* derivativeKernel = nullptr;
	int kernelLength = 0;
	int kernelRadius = 0;

	// for display
	uint32_t* waterPixels = nullptr;
	Texture waterTexture;

	int get_idx(int x, int y) const;
	int get_kernel_reflected_idx(int x, int y) const;

	float get_height(int x, int y) const;
	float get_obstruction(int x, int y) const;
public:
	float velocityDamping;
	float accelerationTerm;

	IWaveSurface(int w, int h, int p);
	~IWaveSurface();

	void place_source(int x, int y, float r, float strength) override;
	void set_obstruction(int x, int y, int rx, int ry, float strength) override;
	void sim_frame(float delta) override;

	Texture get_display() override;

	void reset() override;
};