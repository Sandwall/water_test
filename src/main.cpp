#include <raylib.h> // purely being used for drawing the water

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include <algorithm>

#include "iwave.h"

constexpr int screenWidth = 640, screenHeight = 480;
constexpr int simWidth = screenWidth / 4;
constexpr int simHeight = screenHeight / 4;

int main(int argc, char** argv) {
	InitWindow(screenWidth, screenHeight, "water test");
	ClearWindowState(FLAG_WINDOW_RESIZABLE);
	SetTargetFPS(60);

	IWaveSurface surface(simWidth, simHeight, 6);

	// create grayscale water texture
	Texture waterTexture;
	uint8_t* waterPixels = (uint8_t*)malloc(simWidth * simHeight);
	{
		Image waterImage = GenImageColor(simWidth, simHeight, BLACK);
		ImageFormat(&waterImage, PIXELFORMAT_UNCOMPRESSED_GRAYSCALE);
		waterTexture = LoadTextureFromImage(waterImage);
		UnloadImage(waterImage);
	}

	// main loop
	while (!WindowShouldClose())
	{
		Vector2 mousePos = GetMousePosition();
		int simX = static_cast<int>(mousePos.x / static_cast<float>(screenWidth) * static_cast<float>(simWidth));
		int simY = static_cast<int>(mousePos.y / static_cast<float>(screenHeight) * static_cast<float>(simHeight));

		if (IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
			for(int y = -5; y <= 5; y++) {
				for (int x = -5; x <= 5; x++) {
					surface.place_source(simX + x, simY + y);
				}
			}
			surface.place_source(simX, simY);
		}

		if (IsKeyPressed(KEY_SPACE)) {
			surface.reset();
		}

		surface.sim_frame(1.0f / 30.0f);

		//printf("%f\n", surface.sample(simX, simY));

		// update water texture
		float extents = 5.0f;
		if (waterPixels) {
			for (int y = 0; y < simHeight; y++) {
				for (int x = 0; x < simWidth; x++) {
					uint8_t& pixel = waterPixels[x + (y * simWidth)];
					float surfaceValue = std::clamp(surface.sample(x, y), -extents, extents);

					// the entire screen will be gray when each surface value is at 0.0f
					pixel = static_cast<uint8_t>((((surfaceValue + extents) / (extents * 2.0f)) * 255.0f) + 0.5f);
				}
			}
		}

		UpdateTexture(waterTexture, waterPixels);

		BeginDrawing();
		ClearBackground(RAYWHITE);
		DrawTexturePro(waterTexture,
			Rectangle{ 0, 0, simWidth, simHeight },
			Rectangle{ 0, 0, screenWidth, screenHeight },
			Vector2(0, 0), 0.0f,
			WHITE
		);

		int fps = GetFPS();
		char fpsString[32] = { 0 };
		snprintf(fpsString, 24, "FPS: %d\0", fps);
		DrawText(fpsString, 16, 16, 32, PURPLE);
		EndDrawing();
	}

	CloseWindow();
	return 0;
}