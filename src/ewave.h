#pragma once

#include "surface_sim.h"

// TODO: I'll get to this eventually
// https://people.computing.clemson.edu/~jtessen/students/goswami_thesis.pdf
class EWaveSurface : SurfaceSim {
	int width, height;
public:
	EWaveSurface(int w, int h);
	~EWaveSurface();

	void place_source(int x, int y, float r, float strength) override;
	void set_obstruction(int x, int y, float r, float strength) override;

	void sim_frame(float delta) override;

	void reset() override;
	GLuint get_display() override;
};