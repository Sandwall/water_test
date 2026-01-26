#pragma once

#include "surface_sim.h"
#include "gl_renderer.h"

#define EWAVESURFACE_CPU

// https://people.computing.clemson.edu/~jtessen/students/goswami_thesis.pdf
class EWaveSurface : public SurfaceSim {
	int width, height;
	float gravity = 1.0f; // this is g in the paper

	float* obstruction = nullptr;
	float* velocityPotential = nullptr; // this is phi in the paper
	float* heightGrid = nullptr; // this is h in the paper

	// 
	float* fourierVelocity = nullptr;
	float* fourierHeight = nullptr;


	uint32_t* displayPixels;
	TextureTarget displayTexture;

public:
	EWaveSurface(int w, int h);
	~EWaveSurface();

	void place_source(int x, int y, float r, float strength) override;
	void set_obstruction(int x, int y, float r, float strength) override;

	void sim_frame(float delta) override;

	void reset() override;
	GLuint get_display() override;
};