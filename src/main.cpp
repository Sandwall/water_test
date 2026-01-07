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

uint8_t pix_from_normalized(float value) {
	return static_cast<uint8_t>((value * 255.0f) + 0.5f);
}

int main(int argc, char** argv) {
	InitWindow(screenWidth, screenHeight, "water test");
	ClearWindowState(FLAG_WINDOW_RESIZABLE);
	SetTargetFPS(60);

	IWaveSurface surface(simWidth, simHeight, 6);

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
	while (!WindowShouldClose())
	{
		Vector2 mousePos = GetMousePosition();
		int simX = static_cast<int>(mousePos.x / static_cast<float>(screenWidth) * static_cast<float>(simWidth));
		int simY = static_cast<int>(mousePos.y / static_cast<float>(screenHeight) * static_cast<float>(simHeight));


		if (IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
			for(int y = -5; y <= 5; y++) {
				for (int x = -5; x <= 5; x++) {
					float r = std::max(0.0f, 5.0f - sqrtf(static_cast<float>((y * y) + (x * x))));
					surface.place_source(simX + x, simY + y, r);
				}
			}
		} else if (IsMouseButtonDown(MOUSE_BUTTON_RIGHT)) {
			for (int y = -2; y <= 2; y++) {
				for (int x = -2; x <= 2; x++) {
					//float r = sqrtf(static_cast<float>((y * y) + (x * x))) / 5.0f;
					//if(r < 1.0f)
					surface.set_obstruction(simX + x, simY + y, 0.0f);
				}
			}
		}

		if (IsKeyPressed(KEY_SPACE)) {
			surface.reset();
		}

		surface.sim_frame(1.0f / 30.0f);

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

		int fps = GetFPS();
		char fpsString[32] = { 0 };
		snprintf(fpsString, 24, "FPS: %d\0", fps);
		DrawText(fpsString, 16, 16, 32, PURPLE);
		EndDrawing();
	}

	CloseWindow();
	return 0;
}