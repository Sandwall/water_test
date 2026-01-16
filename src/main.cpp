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
#include <chrono> // for framerate limiting... don't want to make a wrapper function for different platforms

#include "gl_renderer.h"
//#include "iwave.h"
#include "iwave_gpu.h"

#if defined(IWAVESURFACE_GPU)
// GPU version is still very much incomplete
#define IWaveSurfaceObject IWaveSurfaceGPU
#elif defined(IWAVESURFACE_CPU)
#define IWaveSurfaceObject IWaveSurface
#endif

int screenWidth = 1280, screenHeight = 720;
const int divFactor = 1;
int simWidth = screenWidth / divFactor;
int simHeight = screenHeight / divFactor;
bool guiOpen = true;

// currently a leftover from the previous raylib iteration, which had functionality to set target FPS
// TODO: implement frame limiting, currently the program is limited by refresh rate because of vsync
constexpr int targetFps = 60;
constexpr double targetFrameTime = 1.0f / static_cast<double>(targetFps);
double frameTime = targetFrameTime;

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

int do_init();
void do_cleanup();
void imgui_builder(bool* open);

int main(int argc, char** argv) {
	if (0 != do_init())
		return -1;

	Renderer::init();
	IWaveSurfaceObject surface(simWidth, simHeight, 6);

	while (!glfwWindowShouldClose(window)) {
		double startTime = glfwGetTime();

		glfwPollEvents();
		if (0 != glfwGetWindowAttrib(window, GLFW_ICONIFIED)) {
			ImGui_ImplGlfw_Sleep(10);
			continue;
		}

		// I'm just gonna use ImGui's input because a proper input system isn't really a priority here...
		ImGuiIO& io = ImGui::GetIO();
		
		//
		// update
		//
		Point2 simPos(static_cast<int>(io.MousePos.x) / divFactor, static_cast<int>(io.MousePos.y) / divFactor);

		if (!io.WantCaptureMouse) {
			if (io.MouseDown[0]) {
				surface.place_source(simPos.x, simPos.y, 50.0f, 1.0f);
			} else if (io.MouseDown[1]) {
				surface.set_obstruction(simPos.x, simPos.y, 25.0f, 1.0f);
			}
		}

		if (!io.WantCaptureKeyboard) {
			if (ImGui::IsKeyPressed(ImGuiKey_Backslash, false)) {
				guiOpen = !guiOpen;
			}

			if (ImGui::IsKeyPressed(ImGuiKey_Space, false)) {
				surface.reset();
			}
		}

		surface.sim_frame(static_cast<float>(targetFrameTime));
		
		//if (frameTime > targetFrameTime)
		//	surface.sim_frame(targetFrameTime);
		//else
		//	surface.sim_frame(frameTime);

		ImGui_ImplOpenGL3_NewFrame();
		ImGui_ImplGlfw_NewFrame();
		ImGui::NewFrame();

		imgui_builder(&guiOpen);
		surface.imgui_builder(&guiOpen);
		
		ImGui::Render();

		glBindFramebuffer(GL_FRAMEBUFFER, 0);

		glfwGetFramebufferSize(window, &screenWidth, &screenHeight);
		
		glViewport(0, 0, screenWidth, screenHeight);
		
		glClearColor(1.0, 0.0, 1.0, 1.0);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		// shader location could be cached...
		Renderer::attach_tex(Renderer::flippedShader, Renderer::shader_loc(Renderer::flippedShader, "inputTexture"), surface.get_display(), 0);
		glUseProgram(Renderer::flippedShader);
		Renderer::draw_quad();
		Renderer::bind_tex(0, 0);

		ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
		glfwSwapBuffers(window);

		glFinish(); // blocks until GPU commands are done
		frameTime = glfwGetTime() - startTime;

		if (frameTime < targetFrameTime) {
			int sleepMs = static_cast<int>(1000.0f * (targetFrameTime - frameTime));
			ImGui_ImplGlfw_Sleep(sleepMs);
		}
	}

	do_cleanup();
	return 0;
}

static void imgui_builder(bool* open) {
	ImGuiIO& io = ImGui::GetIO();

	if (open && *open) {
		if (ImGui::Begin("Details"), open, ImGuiWindowFlags_AlwaysAutoResize) {
			ImGui::LabelText("Render Frame Time", "%f ms", frameTime * 1000.0f);
			ImGui::LabelText("Target Frame Time", "%f ms", targetFrameTime * 1000.0);
			ImGui::LabelText("Render FPS", "%f fps", frameTime != 0.0f ? 1.0f / frameTime : 0.0f);
			ImGui::LabelText("Target FPS", "%d fps", targetFps);
			ImGui::LabelText("Mouse Pos", "%f %f", io.MousePos.x, io.MousePos.y);
			ImGui::LabelText("LBM Down", io.MouseDown[0] ? "True" : "False");
			ImGui::LabelText("RBM Down", io.MouseDown[2] ? "True" : "False");
		}
		ImGui::End();
	}

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
	glfwSwapInterval(0);

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
	ImGui_ImplOpenGL3_Init("#version 460 core");

	return 0;
}

static inline void do_cleanup() {
	ImGui_ImplOpenGL3_Shutdown();
	ImGui_ImplGlfw_Shutdown();
	ImGui::DestroyContext();

	glfwDestroyWindow(window);
	glfwTerminate();
}
