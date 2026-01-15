#include "gl_renderer.h"

#include <GL/gl3w.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>

void TextureTarget::init(int w, int h, int format = GL_R32F_EXT) {
	clean();

	width = w;
	height = h;
	texture = Renderer::create_only_tex(w, h, format);

	glCreateFramebuffers(1, &framebuffer);
	glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texture, 0);
	
	// the one texture in this struct will be the only color attachment, and we have no need for depth/stencil buffer
	// in addition, we're going to attach the texture as a texture2D because other shaders will be sampling them
	
	GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
	if (status != GL_FRAMEBUFFER_COMPLETE)
		fprintf(stderr, "Framebuffer creation error: ");

	switch (status) {
	case GL_FRAMEBUFFER_UNDEFINED:
		fprintf(stderr, "Framebuffer doesn't exist?\n");
		break;
	case GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT:
		fprintf(stderr, "Framebuffer has incomplete attachments\n");
		break;
	case GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT:
		fprintf(stderr, "Framebuffer does not have at least one attachment\n");
		break;
	case GL_FRAMEBUFFER_UNSUPPORTED:
		fprintf(stderr, "Attached images have unsupported formats\n");
		break;
	}

	glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void TextureTarget::clean() {
	glDeleteFramebuffers(1, &framebuffer);
	glDeleteTextures(1, &texture);

	width = 0;
	height = 0;
	texture = 0;
	framebuffer = 0;
}

void TextureTarget::copy_from(const TextureTarget& from) {
	glBindFramebuffer(GL_READ_BUFFER, from.framebuffer);
	glCopyTextureSubImage2D(texture, 0, 0, 0, 0, 0, width, height);
	glBindFramebuffer(GL_READ_BUFFER, 0);
}

// we probably won't use all of these texture formats, but the initialization for them are here if we ever do
static int byte_size(int format) {
	switch (format) {
	case GL_R32F:
		return sizeof(float);
	case GL_RG32F:
		return 2 * sizeof(float);
	case GL_RGB32F:
		return 3 * sizeof(float);
	case GL_RGBA32F:
		return 4 * sizeof(float);
	case GL_R8:
		return sizeof(uint8_t);
	case GL_RG8:
		return 2 * sizeof(uint8_t);
	case GL_RGB8:
		return 3 * sizeof(uint8_t);
	case GL_RGBA8:
		return 4 * sizeof(uint8_t);
	case GL_R16F:
		return sizeof(uint16_t);
	case GL_RG16F:
		return 2 * sizeof(uint16_t);
	case GL_RGB16F:
		return 3 * sizeof(uint16_t);
	case GL_RGBA16F:
		return 4 * sizeof(uint16_t);
	default:
		return 0;
	}
}

// used for both fullscreen draws and transformed draws (transformation is happening CPU-side)
const char* vertexSource = /* vertex shader */ R"(
#version 430
in vec2 vertexPosition;
in vec2 vertexTexCoord;

out vec2 fragUv;
out vec2 screenUv;

void main() {
	// flipping vertical texture coordinates
	fragUv = vec2(vertexTexCoord.x, 1.0 - vertexTexCoord.y);
	screenUv = (vertexPosition + 1.0) / 2.0;
	screenUv.y = 1.0 - screenUv.y;
	
	gl_Position = vec4(vertexPosition, 0.0, 1.0);
}
)";

// used to copy a texture to another
const char* sampleTextureFragSource = /* fragment shader */ R"(
#version 430
in vec2 fragUv;
in vec2 screenUv;

out vec4 outColor;

uniform sampler2D inputTexture;

void main() {
	outColor = texture(inputTexture, fragUv);
}
)";

GLuint Renderer::tQuadVao;
GLuint Renderer::tQuadVbo;
GLuint Renderer::quadVao;
GLuint Renderer::quadVbo;
GLuint Renderer::regularShader;

void Renderer::init() {
	float vertices[] = {
		// First triangle
		-1.0f,  1.0f, 0.0f, 1.0f,  // Top-left
		-1.0f, -1.0f, 0.0f, 0.0f,  // Bottom-left
		 1.0f, -1.0f, 1.0f, 0.0f,  // Bottom-right

		 // Second triangle
		 -1.0f,  1.0f, 0.0f, 1.0f,  // Top-left
		  1.0f, -1.0f, 1.0f, 0.0f,  // Bottom-right
		  1.0f,  1.0f, 1.0f, 1.0f,  // Top-right
	};

	// create quad vao and quad vbo
	glGenVertexArrays(1, &quadVao);
	glBindVertexArray(quadVao);
	glGenBuffers(1, &quadVbo);
	glBindBuffer(GL_ARRAY_BUFFER, quadVbo);
	glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
	glEnableVertexAttribArray(0);
	glVertexAttribPointer(0, 2, GL_FLOAT, false, sizeof(float) * 4, (void*)0);
	glEnableVertexAttribArray(1);
	glVertexAttribPointer(1, 2, GL_FLOAT, false, sizeof(float) * 4, (void*)(sizeof(float) * 2));
	glBindVertexArray(0);

	// create transformed quad vao and vbo... this will be a dynamic draw vbo
	glGenVertexArrays(1, &tQuadVao);
	glBindVertexArray(tQuadVao);
	glGenBuffers(1, &tQuadVbo);
	glBindBuffer(GL_ARRAY_BUFFER, tQuadVbo);
	glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_DYNAMIC_DRAW);
	glEnableVertexAttribArray(0);
	glVertexAttribPointer(0, 3, GL_FLOAT, false, sizeof(float) * 5, (void*)0);
	glEnableVertexAttribArray(1);
	glVertexAttribPointer(1, 2, GL_FLOAT, false, sizeof(float) * 5, (void*)(sizeof(float) * 3));
	glBindVertexArray(0);

	regularShader = compile_shader(vertexSource, sampleTextureFragSource);
}

void Renderer::cleanup() {

}

GLuint Renderer::create_only_tex(int w, int h, int format, int type, const void* data) {
	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, 0);

	GLuint tex = 0;
	glGenTextures(1, &tex);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

	glBindTexture(GL_TEXTURE_2D, tex);

	if (!data) {
		void* initialData = calloc(1, w * h * byte_size(format));
		glTexImage2D(GL_TEXTURE_2D, 0, format, w, h, 0, format, type, initialData);
		free(initialData);
	}
	else {
		glTexImage2D(GL_TEXTURE_2D, 0, format, w, h, 0, format, type, data);
	}

	glBindTexture(GL_TEXTURE_2D, 0);

	return tex;
}

GLuint Renderer::compile_shader(const char* vs, const char* fs) {
	GLuint vertexShader = glCreateShader(GL_VERTEX_SHADER);
	GLuint fragShader = glCreateShader(GL_FRAGMENT_SHADER);

	GLint vertexLen = strlen(vs);
	GLint fragLen = strlen(vs);

	glShaderSource(vertexShader, 1, &vs, &vertexLen);
	glShaderSource(fragShader, 1, &fs, &fragLen);

	glCompileShader(vertexShader);
	glCompileShader(fragShader);

	GLuint program = glCreateProgram();
	glAttachShader(program, vertexShader);
	glAttachShader(program, fragShader);
	glLinkProgram(program);

	glDeleteShader(vertexShader);
	glDeleteShader(fragShader);

	return program;
}

//
// these functions exist more for organizational purposes than functional
//
void Renderer::bind_tex(GLuint textureSlot, GLuint texture) {
	glActiveTexture(GL_TEXTURE0 + textureSlot);
	glBindTexture(GL_TEXTURE_2D, texture);
}

void Renderer::uniform_tex(GLuint shader, GLuint texture, const char* location) {
	GLuint loc = glGetUniformLocation(shader, location);
	glUseProgram(shader);
	glUniform1i(loc, texture);
}

void Renderer::draw_quad() {
	glDisable(GL_DEPTH_TEST);
	glDisable(GL_CULL_FACE);

	glBindVertexArray(quadVao);
	glDrawArrays(GL_TRIANGLES, 0, 6);
	glBindVertexArray(0);
	
	glEnable(GL_CULL_FACE);
	glEnable(GL_DEPTH_TEST);
}

void Renderer::draw_transformed_quad(float x, float y, float w, float h) {
	glBindVertexArray(tQuadVao);

	// transform the vertices on the CPU and update the buffer

	glDrawArrays(GL_TRIANGLES, 0, 6);
	glBindVertexArray(0);
}