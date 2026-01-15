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
#include "iwave.h"
//#include "iwave_gpu.h"

int screenWidth = 640, screenHeight = 360;
const int divFactor = 4;
int simWidth = screenWidth / divFactor;
int simHeight = screenHeight / divFactor;

constexpr int targetFps = 30;
constexpr float targetFrameTime = 1.0f / static_cast<float>(targetFps);
float frameTime = targetFrameTime;


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
Point2 mousePos;

void imgui_builder();

int do_init();
void do_cleanup();

int main(int argc, char** argv) {
	if (0 != do_init())
		return -1;

	IWaveSurface surface(simWidth, simHeight, 6);
	Renderer::init();

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
		mousePos = io.MousePos;

		// update
		Point2 simPos(mousePos.x / divFactor, mousePos.y / divFactor);

		if (!io.WantCaptureMouse) {
			if (io.MouseDown[0]) {
				surface.place_source(simPos.x, simPos.y, 5.0f, 1.0f);
			}
			else if (io.MouseDown[2]) {
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
		ImGui::Render();

		GLuint displayTex = surface.get_display();
		glBindFramebuffer(GL_FRAMEBUFFER, 0);
		glfwGetFramebufferSize(window, &screenWidth, &screenHeight);
		glViewport(0, 0, screenWidth, screenHeight);
		glClearColor(1.0, 0.0, 1.0, 1.0);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		Renderer::uniform_tex(Renderer::regularShader, 0, "inputTexture");
		Renderer::bind_tex(0, displayTex);
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
		ImGui::LabelText("Mouse Pos", "%d %d", mousePos.x, mousePos.y);
		ImGui::LabelText("LBM Down", io.MouseDown[0] ? "True" : "False");
		ImGui::LabelText("RBM Down", io.MouseDown[2] ? "True" : "False");
	}
	ImGui::End();
}

// initialization & cleanup functions for organization

static inline int do_init() {
	if (GLFW_TRUE != glfwInit()) {
		fprintf(stderr, "Error: GLFW initialization failed!\n");
		return -1;
	}

	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

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
