#include <GL/gl3w.h>
#include <GLFW/glfw3.h>

#include "external/imgui.h"
#include "external/imgui_impl_glfw.h"
#include "external/imgui_impl_opengl3.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include <algorithm>

#include "gl_renderer.h"
//#include "iwave.h"
#include "iwave_gpu.h"

#if defined(IWAVESURFACE_GPU)
#define IWaveSurfaceObject IWaveSurfaceGPU
#elif defined(IWAVESURFACE_CPU)
#define IWaveSurfaceObject IWaveSurface
#endif

int screenWidth = 1280, screenHeight = 720;
const int divFactor = 8;
int simWidth = screenWidth / divFactor;
int simHeight = screenHeight / divFactor;

constexpr int targetFps = 30;
constexpr float targetFrameTime = 1.0f / static_cast<float>(targetFps);
float frameTime = targetFrameTime;

#define print_err(x) fprintf(stderr, x);

struct Point2 {
	int x, y;

	Point2() {
		x = 0;
		y = 0;
	}

	Point2(int px, int py) {
		x = px;
		y = py;
	}

	void operator=(const ImVec2& vec) {
		x = static_cast<int>(vec.x);
		y = static_cast<int>(vec.y);
	}
};

GLFWwindow* window = nullptr;

void imgui_builder();

int do_init();
void do_cleanup();

int main(int argc, char** argv) {
	if (0 != do_init())
		return -1;

	Renderer::init();
	IWaveSurfaceObject surface(simWidth, simHeight, 6);

	double currentTime = glfwGetTime();
	double prevTime = currentTime - targetFrameTime;
	while (!glfwWindowShouldClose(window)) {
		prevTime = currentTime;
		currentTime = glfwGetTime();
		frameTime = static_cast<float>(currentTime - prevTime);

		glfwPollEvents();
		if (0 != glfwGetWindowAttrib(window, GLFW_ICONIFIED)) {
			ImGui_ImplGlfw_Sleep(10);
			continue;
		}

		// I'm just gonna use ImGui's input because a proper input system isn't really a priority here...
		ImGuiIO& io = ImGui::GetIO();

		// update
		Point2 simPos(static_cast<int>(io.MousePos.x) / divFactor, static_cast<int>(io.MousePos.y) / divFactor);


		if (!io.WantCaptureMouse) {
			if (io.MouseDown[0]) {
				surface.place_source(simPos.x, simPos.y, 5.0f, 1.0f);
			} else if (io.MouseDown[2]) {
				surface.set_obstruction(simPos.x, simPos.y, 2.0f, 0.0f);
			}
		}

		if (!io.WantCaptureKeyboard) {
			if (ImGui::IsKeyPressed(ImGuiKey_Space, false)) {
				surface.reset();
			}
		}

		if (frameTime > targetFrameTime)
			surface.sim_frame(targetFrameTime);
		else
			surface.sim_frame(frameTime);

		ImGui_ImplOpenGL3_NewFrame();
		ImGui_ImplGlfw_NewFrame();
		ImGui::NewFrame();

		imgui_builder();
		surface.imgui_builder();
		
		ImGui::Render();

		glBindFramebuffer(GL_FRAMEBUFFER, 0);

		glfwGetFramebufferSize(window, &screenWidth, &screenHeight);
		
		glViewport(0, 0, screenWidth, screenHeight);
		
		glClearColor(1.0, 0.0, 1.0, 1.0);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		Renderer::uniform_tex(Renderer::regularShader, 0, Renderer::shader_loc(Renderer::regularShader, "inputTexture"));
		// this could be cached...

		Renderer::bind_tex(0, surface.get_display());
		Renderer::sampler_settings();
		glUseProgram(Renderer::regularShader);
		Renderer::draw_quad();
		Renderer::bind_tex(0, 0);

		ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
		glfwSwapBuffers(window);
	}

	do_cleanup();
	return 0;
}

static inline void imgui_builder() {
	ImGuiIO& io = ImGui::GetIO();

	if (ImGui::Begin("Details")) {
		ImGui::LabelText("Frame Time", "%f ms", frameTime * 1000.0f);
		ImGui::LabelText("FPS", "%f fps", frameTime != 0.0f ? 1.0f / frameTime : 0.0f);
		ImGui::LabelText("Mouse Pos", "%f %f", io.MousePos.x, io.MousePos.y);
		ImGui::LabelText("LBM Down", io.MouseDown[0] ? "True" : "False");
		ImGui::LabelText("RBM Down", io.MouseDown[2] ? "True" : "False");
	}
	ImGui::End();
}

// initialization & cleanup functions for organization

static void APIENTRY glDebugOutput(
	GLenum source, GLenum type, unsigned int id, GLenum severity, GLsizei length,
	const char* message, const void* userParam) {
	(void)length;
	(void)userParam;

	// ignore non-significant error/warning codes
	if (id == 131169 || id == 131185 || id == 131218 || id == 131204)
		return;

	print_err("---------------\n");
	fprintf(stderr, "Debug message (%u): %s\n", id, message);

	switch (source) {
		case GL_DEBUG_SOURCE_API:             print_err("Source: API\n"); break;
		case GL_DEBUG_SOURCE_WINDOW_SYSTEM:   print_err("Source: Window System\n"); break;
		case GL_DEBUG_SOURCE_SHADER_COMPILER: print_err("Source: Shader Compiler\n"); break;
		case GL_DEBUG_SOURCE_THIRD_PARTY:     print_err("Source: Third Party\n"); break;
		case GL_DEBUG_SOURCE_APPLICATION:     print_err("Source: Application\n"); break;
		case GL_DEBUG_SOURCE_OTHER:           print_err("Source: Other\n"); break;
	}

	switch (type) {
		case GL_DEBUG_TYPE_ERROR:               print_err("Type: Error\n"); break;
		case GL_DEBUG_TYPE_DEPRECATED_BEHAVIOR: print_err("Type: Deprecated Behaviour\n"); break;
		case GL_DEBUG_TYPE_UNDEFINED_BEHAVIOR:  print_err("Type: Undefined Behaviour\n"); break;
		case GL_DEBUG_TYPE_PORTABILITY:         print_err("Type: Portability\n"); break;
		case GL_DEBUG_TYPE_PERFORMANCE:         print_err("Type: Performance\n"); break;
		case GL_DEBUG_TYPE_MARKER:              print_err("Type: Marker\n"); break;
		case GL_DEBUG_TYPE_PUSH_GROUP:          print_err("Type: Push Group\n"); break;
		case GL_DEBUG_TYPE_POP_GROUP:           print_err("Type: Pop Group\n"); break;
		case GL_DEBUG_TYPE_OTHER:               print_err("Type: Other\n"); break;
	}

	switch (severity) {
		case GL_DEBUG_SEVERITY_HIGH:         print_err("Severity: high\n"); break;
		case GL_DEBUG_SEVERITY_MEDIUM:       print_err("Severity: medium\n"); break;
		case GL_DEBUG_SEVERITY_LOW:          print_err("Severity: low\n"); break;
		case GL_DEBUG_SEVERITY_NOTIFICATION: print_err("Severity: notification\n"); break;
	}

	print_err("\n");
}

static inline int do_init() {
	if (GLFW_TRUE != glfwInit()) {
		fprintf(stderr, "Error: GLFW initialization failed!\n");
		return -1;
	}

	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
	glfwWindowHint(GLFW_OPENGL_DEBUG_CONTEXT, GL_TRUE);

	window = glfwCreateWindow(screenWidth, screenHeight, "water test", nullptr, nullptr);
	if (window == nullptr) return -1;

	glfwSetWindowAttrib(window, GLFW_RESIZABLE, GLFW_FALSE);
	glfwMakeContextCurrent(window);
	glfwSwapInterval(1);

	if (gl3wInit()) {
		fprintf(stderr, "Error: GL3W initialization failed!\n");
		return -1;
	}

	if (!gl3wIsSupported(4, 6)) {
		fprintf(stderr, "Error: OpenGL 4.6 is not supported!\n");
		return -1;
	}

	int flags; glGetIntegerv(GL_CONTEXT_FLAGS, &flags);
	if (flags & GL_CONTEXT_FLAG_DEBUG_BIT) {
		glEnable(GL_DEBUG_OUTPUT);
		glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
		glDebugMessageCallback(glDebugOutput, nullptr);
		glDebugMessageControl(GL_DONT_CARE, GL_DONT_CARE, GL_DONT_CARE, 0, nullptr, GL_TRUE);
	}

	// Setup Dear ImGui
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO(); (void)io;
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
	io.IniFilename = nullptr;
	ImGui::StyleColorsDark();

	ImGui_ImplGlfw_InitForOpenGL(window, true);
	ImGui_ImplOpenGL3_Init("#version 460");

	return 0;
}

static inline void do_cleanup() {
	ImGui_ImplOpenGL3_Shutdown();
	ImGui_ImplGlfw_Shutdown();
	ImGui::DestroyContext();

	glfwDestroyWindow(window);
	glfwTerminate();
}
