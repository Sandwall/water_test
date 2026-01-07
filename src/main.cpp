#include <raylib.h> // purely being used for drawing the water

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include <algorithm>

#include "iwave.h"

constexpr int screenWidth = 1280, screenHeight = 720;
constexpr int divFactor = 8;
constexpr int simWidth = screenWidth / divFactor;
constexpr int simHeight = screenHeight / divFactor;

constexpr int targetFps = 60;
constexpr float targetFrameTime = 1.0f / static_cast<float>(targetFps);

uint8_t pix_from_normalized(float value) {
	return static_cast<uint8_t>((value * 255.0f) + 0.5f);
}

Vector2 mousePos, prevMousePos;

struct Point2 {
	int x;
	int y;
};

int main(int argc, char** argv) {
	InitWindow(screenWidth, screenHeight, "water test");
	ClearWindowState(FLAG_WINDOW_RESIZABLE);
	SetTargetFPS(60);

	IWaveSurface surface(simWidth, simHeight, 4);

	// create grayscale water texture
	Texture waterTexture;
	uint32_t* waterPixels = (uint32_t*)malloc(sizeof(uint32_t) * simWidth * simHeight);
	{
		Image waterImage = GenImageColor(simWidth, simHeight, BLACK);
		ImageFormat(&waterImage, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8);
		waterTexture = LoadTextureFromImage(waterImage);
		UnloadImage(waterImage);
	}

	// main loop
	mousePos = GetMousePosition();
	while (!WindowShouldClose())
	{
		prevMousePos = mousePos;
		mousePos = GetMousePosition();

		Point2 simPos = {
			static_cast<int>(mousePos.x / static_cast<float>(screenWidth) * static_cast<float>(simWidth)),
			static_cast<int>(mousePos.y / static_cast<float>(screenHeight) * static_cast<float>(simHeight))
		};

		if (IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
			for (int y = -5; y <= 5; y++) {
				for (int x = -5; x <= 5; x++) {
					float r = 5.0f - sqrtf(static_cast<float>((y * y) + (x * x)));
					if (r > 0.0f) {
						surface.place_source(simPos.x + x, simPos.y + y, r);
					}
				}
			}
		} else if (IsMouseButtonDown(MOUSE_BUTTON_RIGHT)) {
			for (int y = -2; y <= 2; y++) {
				for (int x = -2; x <= 2; x++) {
					//float r = sqrtf(static_cast<float>((y * y) + (x * x))) / 5.0f;
					//if(r < 1.0f)
					surface.set_obstruction(simPos.x + x, simPos.y + y, 0.0f);
				}
			}
		}

		if (IsKeyPressed(KEY_SPACE)) {
			surface.reset();
		}

		float frameTime = GetFrameTime();

		// just so that dragging the window doesn't mess up the simulation
		if(frameTime > targetFrameTime)
			surface.sim_frame(targetFrameTime);
		else
			surface.sim_frame(frameTime);

		BeginDrawing();
		ClearBackground(RAYWHITE);

		// update water texture
		float extents = 5.0f;
		if (waterPixels) {
			for (int y = 0; y < simHeight; y++) {
				for (int x = 0; x < simWidth; x++) {
					uint8_t* pixel = reinterpret_cast<uint8_t*>(&waterPixels[x + (y * simWidth)]);
					uint8_t& red = pixel[0];
					uint8_t& green = pixel[1];
					uint8_t& blue = pixel[2];
					uint8_t& alpha = pixel[3];

					green = 0;
					alpha = 255;

					float surfaceValue = std::clamp(surface.get_height(x, y), -extents, extents);
					// the entire screen will be half blue when each surface value is at 0.0f
					blue = pix_from_normalized((surfaceValue + extents) / (extents * 2.0f));

					red = pix_from_normalized(1.0f - surface.get_obstruction(x, y));
				}
			}
		}
		UpdateTexture(waterTexture, waterPixels);
		DrawTexturePro(waterTexture,
			Rectangle{ 0, 0, simWidth, simHeight },
			Rectangle{ 0, 0, screenWidth, screenHeight },
			Vector2(0, 0), 0.0f,
			WHITE
		);

		char fpsString[32] = { 0 };
		snprintf(fpsString, 31, "%f ms", frameTime);
		DrawText(fpsString, 16, 16, 32, PURPLE);
		EndDrawing();
	}

	CloseWindow();
	return 0;
}