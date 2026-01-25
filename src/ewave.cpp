#include "ewave.h"

#include <stdlib.h> // for calloc/free
#include <math.h>
#include <algorithm>

#include <GL/gl3w.h>
#include "gl_renderer.h"

// NOTE: i'm lazy lol
#define DOALLOC static_cast<float*>(malloc(bufferSize))
#define DOFREE(x) free(x); x = nullptr;
#define SETZERO(x) memset(x, 0, bufferSize)

EWaveSurface::EWaveSurface(int w, int h) {

}

EWaveSurface::~EWaveSurface() {

}
void EWaveSurface::place_source(int x, int y, float r, float strength) {

}

void EWaveSurface::set_obstruction(int x, int y, float r, float strength) {

}

// the simulation happens entirely in fourier space, which means that get_display
// has to do an inverse fourier transform to get back to spatial data
void EWaveSurface::sim_frame(float delta) {

}

void EWaveSurface::reset() {

}

GLuint EWaveSurface::get_display() {


	return 0;
}