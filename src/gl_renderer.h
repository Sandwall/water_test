#pragma once

#include <GL/glcorearb.h> // for GL types


struct TextureTarget {
	GLuint framebuffer;
	GLuint texture;
	int width, height;

	void init(int w, int h, int format = GL_R32F);
	void set_target();
	void clean();

	void copy_from(const TextureTarget& from);
};

// this project is simple enough that I'll just leave this as a singleton
class Renderer {
	static GLuint tQuadVao, tQuadVbo;
	static GLuint quadVao, quadVbo;
public:
	static const char* vertexSource;
	static GLuint regularShader;

	static void init(); // requires all GL initialization is done
	static void cleanup();

	// turns unsized/sized format + type into pixel size in bytes
	static int byte_size(int format, int type);

	// pass in sized format (which is used for the internal format)
	// get unsized format + channel type
	static void get_format_info(int internalFormat, int& dataFormat, int& dataType);

	// used to organize a few opengl calls
	static void sampler_settings();
	static GLuint create_tex(int w, int h, int format = GL_R32F, const void* data = nullptr);
	static GLuint compile_shader(const char* vs, const char* fs);
	static void bind_tex(GLuint textureSlot, GLuint texture);
	static GLuint shader_loc(GLuint shader, const char* location);
	static void uniform_tex(GLuint shader, GLuint texture, GLuint location);

	static void draw_quad();
	static void draw_transformed_quad(float x, float y, float w, float h);
};