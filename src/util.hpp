#pragma once

template <typename T, unsigned int N>
class RollingAverageSmoother {
	T samples[N];
	int total;
	int current;
public:
	RollingAverageSmoother() { clear(); }

	void push(T t) {
		total++;
		if (total >= N)
			total = N;

		samples[current % N] = t;

		current = (current + 1) % N;
	}

	T get() {
		T sum = 0;

		for (int i = 0; i < total; i++)
			sum += samples[i];

		return sum / static_cast<T>(total);
	}

	void clear() {
		total = 0;
		current = 0;

		for (int i = 0; i < N; i++)
			samples[i] = 0;
	}
};

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
};