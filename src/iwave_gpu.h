#pragma once

#pragma once

#include "surface_sim.h"
#include <raylib.h>

// NOTE: this does nothing currently
// TODO: change grids and sourceObstruct to RenderTexture
//       i don't think raylib lets you render to regular textures lol


// https://people.computing.clemson.edu/~jtessen/reports/papers_files/Interactive_Water_Surfaces.pdf
class IWaveSurfaceGPU : public SurfaceSim {
	int width = 0, height = 0;

	// going to be swapping between
	int currentGrid = 0;
	Texture grids[3];
	Texture sourceObstruct;    // 4-channel float texture for auxilliary data
	                           // r = source, g = obstruction

	Shader simShader;

	int kernelRadius = 0;
	Texture kernel;
	Texture compute_kernel(int radius);
	//float* derivativeKernel = nullptr;
	//int kernelLength = 0;

public:
	float velocityDamping;
	float accelerationTerm;

	IWaveSurfaceGPU(int w, int h, int p);
	~IWaveSurfaceGPU();

	void place_source(int x, int y, float r, float strength) override;
	void set_obstruction(int x, int y, int rx, int ry, float strength) override;
	void sim_frame(float delta) override;
	void reset() override;
	Texture* get_display() override;
};