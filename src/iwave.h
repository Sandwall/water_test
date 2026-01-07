#pragma once

// https://people.computing.clemson.edu/~jtessen/reports/papers_files/Interactive_Water_Surfaces.pdf
class IWaveSurface {
	int width = 0, height = 0;

	float velocityDamping;
	float accelerationTerm;

	float* currentGrid = nullptr;
	float* prevGrid = nullptr;
	float* verticalDerivative = nullptr;
	float* source = nullptr;
	float* obstruction = nullptr;

	float* derivativeKernel = nullptr;
	int kernelLength = 0;
	int kernelRadius = 0;

	int bufferCount = 0;
	int bufferSize = 0;
public:
	IWaveSurface(int w, int h, int p);
	~IWaveSurface();

	int get_idx(int x, int y) {
		if (x < 0 || x >= width || y < 0 || y >= height)
			return -1;

		return x + (y * width);
	}

	void place_source(int x, int y);
	void sim_frame(float delta);

	float sample(int x, int y);
	void reset();
};