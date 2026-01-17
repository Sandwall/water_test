#pragma once

#pragma once

#include "surface_sim.h"
#include "gl_renderer.h"

#define IWAVESURFACE_GPU

// https://people.computing.clemson.edu/~jtessen/reports/papers_files/Interactive_Water_Surfaces.pdf
class IWaveSurfaceGPU : public SurfaceSim {
	int width = 0, height = 0;

	TextureTarget display;
	GLuint displayShader;
	GLint d_currentGrid, d_sourceObstruct;

	// there are more than 2 grids for pingpong rendering
	TextureTarget currentGrid, prevGrid, pingpongGrid;
	TextureTarget verticalDerivative;

	// 4-channel float texture for auxiliary data
	// r = source, g = obstruction
	TextureTarget sourceObstruct, pingpongSO; 

	// for mutating auxiliary data (sourceObstruct)
	GLuint drawAuxShader;
	GLint n1_sourceObstruct, n1_maxValue;

	GLuint progressSoShader;
	GLint n2_sourceObstruct, n2_speed;

	// progresses the simulation
	GLuint preprocessShader;
	GLint p1_currentGrid, p1_sourceObstruct;
	
	GLuint convolutionShader;
	GLint p2_currentGrid, p2_kernel, p2_kernelCellSize, p2_gridCellSize, p2_kernelRadius;
	
	GLuint propagateShader;
	GLint p3_currentGrid, p3_prevGrid, p3_verticalDerivative, p3_coefficients;

	
	int kernelRadius = 0;
	unsigned int kernelTexture;
	unsigned int compute_kernel(int radius);

	void draw_aux(int x, int y, float r, float v1, float v2);

public:
	float velocityDamping;
	float accelerationTerm;

	IWaveSurfaceGPU(int w, int h, int p);
	~IWaveSurfaceGPU();

	void place_source(int x, int y, float r, float strength) override;
	void set_obstruction(int x, int y, float r, float strength) override;
	void sim_frame(float delta) override;
	void reset() override;
	GLuint get_display() override;

	void imgui_builder(bool* open = nullptr);
};