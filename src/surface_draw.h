#pragma once

#include <GL/gl3w.h>
#include "gl_renderer.h"

class SurfaceDraw {
	size_t numQuads;
	float* planeVertices = nullptr;

	GLuint planeVao, planeVbo;

	GLuint shader;
	GLint transformLoc, heightLoc, surfaceLoc;

	//Matrix4 mvp;
public:
	SurfaceDraw();
	~SurfaceDraw();

	void draw_tex(GLuint texture);
	void gen_plane(int nx, int ny);

	void clean();
};