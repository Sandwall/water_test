#pragma once

#include <GL/glcorearb.h> // for GL types

struct TextureTarget {
	GLuint framebuffer;
	GLuint texture;
	int width, height;

	void init(int w, int h, int format);
	void clean();

	void copy_from(const TextureTarget& from);
};

// this project is simple enough that I'll just leave this as a singleton
class Renderer {
	static GLuint tQuadVao, tQuadVbo;
	static GLuint quadVao, quadVbo;
public:
	static GLuint regularShader;

	static void init(); // requires all GL initialization is done
	static void cleanup();

	static GLuint create_only_tex(int w, int h, int format = GL_R32F, int type = GL_FLOAT, const void* data = nullptr);
	static GLuint compile_shader(const char* vs, const char* fs);
	static void bind_tex(GLuint textureSlot, GLuint texture);
	static void uniform_tex(GLuint shader, GLuint texture, const char* location);

	static void draw_quad();
	static void draw_transformed_quad(float x, float y, float w, float h);
};