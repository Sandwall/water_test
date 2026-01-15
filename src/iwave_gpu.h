#pragma once

#pragma once

#include "surface_sim.h"
#include "texture_target.h"

// https://people.computing.clemson.edu/~jtessen/reports/papers_files/Interactive_Water_Surfaces.pdf
class IWaveSurfaceGPU : public SurfaceSim {
	int width = 0, height = 0;

	// there are more than 2 grids for pingpong rendering
	TextureTarget currentGrid, prevGrid, pingpongGrid;
	TextureTarget verticalDerivative;

	// 4-channel float texture for auxiliary data
	// r = source, g = obstruction
	TextureTarget sourceObstruct, pingpongSO; 

	Shader copyShader;
	// for mutating auxiliary data (sourceObstruct)
	Shader drawAuxShader;
	Shader progressSoShader;

	// progresses the simulation
	Shader preprocessShader;
	Shader convolutionShader;
	Shader propagateShader;

	int c_inputTexture;

	int n1_sourceObstruct, n1_mvp, n1_maxValue;
	int n2_sourceObstruct, n2_speed;

	int p1_currentGrid, p1_sourceObstruct;
	int p2_currentGrid, p2_kernel, p2_gridCellSize, p2_kernelRadius;
	int p3_currentGrid, p3_prevGrid, p3_verticalDerivative, p3_coefficients;
	
	int kernelRadius = 0;
	unsigned int kernelTexture;
	unsigned int compute_kernel(int radius);

public:
	float velocityDamping;
	float accelerationTerm;

	IWaveSurfaceGPU(int w, int h, int p);
	~IWaveSurfaceGPU();

	void place_source(int x, int y, float r, float strength) override;
	void set_obstruction(int x, int y, float r, float strength) override;
	void sim_frame(float delta) override;
	void reset() override;
	unsigned int get_display() override;
};