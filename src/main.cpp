#include <raylib.h> // purely being used for drawing the water


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include <algorithm>

#include "iwave.h"
#include "iwave_gpu.h"

constexpr int screenWidth = 1280, screenHeight = 720;
constexpr int divFactor = 2;
constexpr int simWidth = screenWidth / divFactor;
constexpr int simHeight = screenHeight / divFactor;

constexpr int targetFps = 30;
constexpr float targetFrameTime = 1.0f / static_cast<float>(targetFps);

Vector2 mousePos, prevMousePos;

struct Point2 {
	int x;
	int y;
};

int main(int argc, char** argv) {
	InitWindow(screenWidth, screenHeight, "water test");
	ClearWindowState(FLAG_WINDOW_RESIZABLE);
	SetTargetFPS(targetFps);

	IWaveSurfaceGPU surface(simWidth, simHeight, 6);

	// main loop
	mousePos = GetMousePosition();
	while (!WindowShouldClose()) {
		prevMousePos = mousePos;
		mousePos = GetMousePosition();

		Point2 simPos = {
			static_cast<int>(mousePos.x / static_cast<float>(screenWidth) * static_cast<float>(simWidth)),
			static_cast<int>(mousePos.y / static_cast<float>(screenHeight) * static_cast<float>(simHeight))
		};

		printf("%d %d\n", simPos.x, simPos.y);

		if (IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
			surface.place_source(simPos.x, simPos.y, 10.0f, 1.0f);
		} else if (IsMouseButtonDown(MOUSE_BUTTON_RIGHT)) {
			surface.set_obstruction(simPos.x, simPos.y, 2.0f, 0.0f);
		}

		if (IsKeyPressed(KEY_SPACE)) {
			surface.reset();
		}

		float frameTime = GetFrameTime();



		BeginDrawing();
		ClearBackground(MAGENTA);

		// just so that dragging the window doesn't mess up the simulation
		if(frameTime > targetFrameTime)
			surface.sim_frame(targetFrameTime);
		else
			surface.sim_frame(frameTime);

		DrawTexturePro(surface.get_display(),
			Rectangle{ 0, 0, simWidth, simHeight },
			Rectangle{ 0, 0, screenWidth, screenHeight },
			Vector2(0, 0), 0.0f, WHITE
		);

		char fpsString[32] = { 0 };
		snprintf(fpsString, 31, "%f ms", frameTime * 1000.0f);
		DrawText(fpsString, 16, 16, 32, PURPLE);
		snprintf(fpsString, 31, "%f fps", 1.0f / frameTime);
		DrawText(fpsString, 16, 48, 32, PURPLE);
		EndDrawing();
	}

	CloseWindow();
	return 0;
}