#pragma once

#include <GL/glcorearb.h> // for GL types
#include <stdint.h>

class SurfaceSim {
public:
	// places source circle at (x, y) with radius r
	virtual void place_source(int x, int y, float r, float strength) = 0;

	// places obstruction square at (x, y), with width 2rx+1 and height 2ry+1
	virtual void set_obstruction(int x, int y, float r, float strength) = 0;

	// simulate a frame
	virtual void sim_frame(float delta) = 0;

	// reset simulation state
	virtual void reset() = 0;

	// uses internal variables to get a texture that can be presented to the screen
	virtual GLuint get_display() = 0;

	// if the simulation wants to display any data in a UI
	virtual void imgui_builder() {}
};

// helper function for displaying
static uint8_t pix_from_normalized(float value) {
	return static_cast<uint8_t>((value * 255.0f) + 0.5f);
}