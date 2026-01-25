#define _USE_MATH_DEFINES

#include "surface_draw.h"

#include "gl_renderer.h"

#include <math.h>
#include <stdlib.h>

const char* surfaceVertexShader = /* vertex shader */ R"(
#version 460 core

layout (location = 0) in vec2 inPos;
layout (location = 1) in vec2 inUv;

out vec4 outPos;
out vec3 outNormal;

uniform mat4 transform;
uniform float heightMultiplier;
uniform vec2 texelSize;
uniform sampler2D surfaceTex;

void main() {
	float surfaceValue = texture(surfaceTex, inUv).r * heightMultiplier;


	outPos = transform * vec4(inPos, surfaceValue, 1.0);
}

)";

const char* surfaceFragShader = /* fragment shader */ R"(
#version 460 core

in vec4 inNormal;

out vec4 outColor;


void main() {
	outColor = vec4(1.0, 0.5, 0.5, 1.0);
}

)";

SurfaceDraw::SurfaceDraw() {
	shader = Renderer::compile_shader(surfaceVertexShader, surfaceFragShader);

	transformLoc = Renderer::shader_loc(shader, "transform");
	heightLoc = Renderer::shader_loc(shader, "heightMultiplier");
	surfaceLoc = Renderer::shader_loc(shader, "surfaceTex");

	planeVao = 0;
	planeVbo = 0;
}

SurfaceDraw::~SurfaceDraw() {

}

void SurfaceDraw::draw_tex(GLuint texture) {
	glUseProgram(shader);
	Renderer::attach_tex(texture, surfaceLoc, texture, 0);

	int viewSize[4];
	glGetIntegerv(GL_VIEWPORT, viewSize);
	float width = static_cast<float>(viewSize[2] - viewSize[0]);
	float height = static_cast<float>(viewSize[3] - viewSize[1]);

	//mvp = Matrix4::perspective(M_PI, width / height, 0.001f, 1000.0f) * Matrix4::transform(0.0f);
	//glUniformMatrix4fv(transformLoc, 1, GL_FALSE, mvp.m);

	glBindVertexArray(planeVao);
	glDrawArrays(GL_TRIANGLES, 0, numQuads * 6);
}

void SurfaceDraw::gen_plane(int nx, int ny) {
	constexpr size_t sizeVertex = 4; // number of floats in 1 vertex
	constexpr size_t sizeQuad = sizeVertex * 6;
	const size_t numQuads = nx * ny;
	const size_t numVertices = sizeQuad * numQuads;

	clean();
	planeVertices = static_cast<float*>(malloc(numVertices * sizeof(float)));


	for (int i = 0; i < numVertices; i += sizeQuad) {
	}
}

void SurfaceDraw::clean() {
	if (planeVertices) {
		free(planeVertices);
		planeVertices = nullptr;
	}

	if (planeVao != 0) {
		glDeleteVertexArrays(1, &planeVao);
		planeVao = 0;
	}

	if (planeVbo != 0) {
		glDeleteBuffers(1, &planeVbo);
		planeVbo = 0;
	}
}