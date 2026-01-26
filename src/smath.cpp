#include "smath.h"

#include <type_traits>
#include <bit>

//
// Structures
//

Matrix4::Matrix4(float diag) {
	for (int i = 0; i < 16; ++i)
		m[i] = 0.0f;

	m[0] = diag;
	m[5] = diag;
	m[10] = diag;
	m[15] = diag;
}

float& Matrix4::operator()(int row, int col) {
	return m[col * 4 + row];
}

const float& Matrix4::operator()(int row, int col) const {
	return m[col * 4 + row];
}

Matrix4 Matrix4::operator+(const Matrix4& rhs) const {
	Matrix4 r(0.0f);
	for (int i = 0; i < 16; ++i)
		r.m[i] = m[i] + rhs.m[i];
	return r;
}

Matrix4& Matrix4::operator+=(const Matrix4& rhs) {
	for (int i = 0; i < 16; ++i)
		m[i] += rhs.m[i];
	return *this;
}

Matrix4 Matrix4::operator-() const {
	Matrix4 r(0.0f);
	for (int i = 0; i < 16; ++i)
		r.m[i] = -m[i];
	return r;
}

Matrix4 Matrix4::operator*(const Matrix4& rhs) const {
	Matrix4 r(0.0f);
	for (int col = 0; col < 4; ++col) {
		for (int row = 0; row < 4; ++row) {
			for (int k = 0; k < 4; ++k) {
				r(row, col) += (*this)(row, k) * rhs(k, col);
			}
		}
	}
	return r;
}

Matrix4& Matrix4::operator*=(const Matrix4& rhs) {
	*this = (*this) * rhs;
	return *this;
}

Matrix4 Matrix4::identity() {
	return Matrix4(1.0f);
}

Matrix4 Matrix4::perspective(float fovyRadians,
	float aspect,
	float zNear,
	float zFar) {
	Matrix4 r(0.0f);
	float f = 1.0f / tanf(fovyRadians * 0.5f);

	r(0, 0) = f / aspect;
	r(1, 1) = f;
	r(2, 2) = (zFar + zNear) / (zNear - zFar);
	r(2, 3) = -1.0f;
	r(3, 2) = (2.0f * zFar * zNear) / (zNear - zFar);

	return r;
}

Matrix4 Matrix4::translation(float x, float y, float z) {
	Matrix4 r(1.0f);
	r(3, 0) = x;
	r(3, 1) = y;
	r(3, 2) = z;
	return r;
}

Matrix4 Matrix4::scale(float x, float y, float z) {
	Matrix4 r(1.0f);
	r(0, 0) = x;
	r(1, 1) = y;
	r(2, 2) = z;
	return r;
}

Matrix4 Matrix4::rotationX(float radians) {
	Matrix4 r(1.0f);
	float c = cosf(radians);
	float s = sinf(radians);

	r(1, 1) = c;
	r(2, 1) = -s;
	r(1, 2) = s;
	r(2, 2) = c;
	return r;
}

Matrix4 Matrix4::rotationY(float radians) {
	Matrix4 r(1.0f);
	float c = cosf(radians);
	float s = sinf(radians);

	r(0, 0) = c;
	r(2, 0) = s;
	r(0, 2) = -s;
	r(2, 2) = c;
	return r;
}

Matrix4 Matrix4::rotationZ(float radians) {
	Matrix4 r(1.0f);
	float c = cosf(radians);
	float s = sinf(radians);

	r(0, 0) = c;
	r(1, 0) = -s;
	r(0, 1) = s;
	r(1, 1) = c;
	return r;
}

Matrix4 Matrix4::transform(float tx, float ty, float tz,
	float rx, float ry, float rz,
	float sx, float sy, float sz) {
	return translation(tx, ty, tz)
		* rotationZ(rz)
		* rotationY(ry)
		* rotationX(rx)
		* scale(sx, sy, sz);
}

namespace smath {
	void* intermediateBuffer = nullptr;
	static constexpr size_t BUFFER_SIZE_COMPLEX = 1 << 16;
	static constexpr size_t BUFFER_SIZE_FLOAT = BUFFER_SIZE_COMPLEX * 2;

	void init() {
		intermediateBuffer = malloc(sizeof(Complex) * BUFFER_SIZE_COMPLEX);
	}

	void cleanup() {
		free(intermediateBuffer);
		intermediateBuffer = nullptr;
	}

	// Transpose functions for 2D float and Complex arrays

	void transpose(size_t x, size_t y, float* matrix) {
		for (int iy = 0; iy < y; iy++) {
			for (size_t ix = iy + 1; ix < x; ix++) {

			}
		}
	}

	void transpose(size_t x, size_t y, Complex* matrix) {

	}

	//
	// Regular Discrete Fourier Transform functions
	// These only exist because I haven't implemented an FFT yet, but I'd still like to work on 
	// parts of the project that require some sort of Fourier Transform routine.
	//

	// 1D Discrete Sine Transform
	void dst(size_t len, float* output, float* input) {
		const float invLen = 1.0f / static_cast<float>(len);

		for (size_t i = 0; i < len; i++) {
			output[i] = 0;

			for (size_t j = 0; j < len; j++) {
				float angle = -smath::tau * static_cast<float>(i * j) * invLen;
				output[i] += input[j] * sinf(angle);
			}
		}
	}

	// 1D Discrete Cosine Transform
	void dct(size_t len, float* output, float* input) {
		const float invLen = 1.0f / static_cast<float>(len);

		for (size_t i = 0; i < len; i++) {
			output[i] = 0;

			for (size_t j = 0; j < len; j++) {
				float angle = -smath::tau * static_cast<float>(i * j) * invLen;
				output[i] += input[j] * cosf(angle);
			}
		}
	}

	// 1D Discrete Fourier Transform
	void dft(size_t len, Complex* output, const Complex* input) {
		const float invLen = 1.0f / static_cast<float>(len);

		for (size_t i = 0; i < len; i++) {
			output[i] = 0;

			for (size_t j = 0; j < len; j++) {
				float angle = -smath::tau * static_cast<float>(i * j) * invLen;
				Complex twiddleFactor(cosf(angle), sinf(angle));
				output[i] += input[j] * twiddleFactor;
			}
		}
	}

	// 2D Discrete Sine Transform
	void dst(size_t x, size_t y, float* output, float* input) {
		float* intermediate = static_cast<float*>(intermediateBuffer);

		// Horizontal Pass
		for (size_t iy = 0; iy < y; iy++) {
			const size_t start = iy * x;
			dst(x, intermediate, &input[start]);
		}

		transpose(x, y, intermediate);
		
		// Vertical Pass
		for (size_t iy = 0; iy < y; iy++) {
			const size_t start = iy * x;
			dst(x, &output[start], intermediate);
		}

		transpose(x, y, output);
	}

	// 2D Discrete Cosine Transform
	//void dct(size_t x, size_t y, float* output, float* input) {

	//	for (size_t i = 0; i < len; i++) {
	//		output[i] = 0;

	//		for (size_t j = 0; j < len; j++) {
	//			float angle = -2.0f * M_PI * static_cast<float>(i * j) * invLen;
	//			output[i] += input[j] * cosf(angle);
	//		}
	//	}
	//}

	//// 2D Discrete Fourier Transform
	//void dft(size_t x, size_t y, Complex* output, const Complex* input) {
	//	const float invLen = 1.0f / static_cast<float>(len);

	//	for (size_t i = 0; i < len; i++) {
	//		output[i] = 0;

	//		for (size_t j = 0; j < len; j++) {
	//			float angle = -2.0f * M_PI * static_cast<float>(i * j) * invLen;
	//			Complex twiddleFactor(cosf(angle), sinf(angle));
	//			output[i] += input[j] * twiddleFactor;
	//		}
	//	}
	//}

	template <typename T> requires std::is_integral_v<T>
	static inline bool is_power_of_2(T val) {
		return (val >= 0) && (0 == (val & (val - 1)));
	}

	template <typename T> requires std::is_integral_v<T>
	static inline int log2i(T val) {
		if (val <= 0) return 0;
		return std::bit_width(val) - 1;
	}

	template <typename T> requires std::is_integral_v<T>
	static inline T pow2i(T val) {
		if (val < 0) return 0;
		if (val == 0) return 1;

		return 1 << val;
	}

	// TODO: need to implement this
	void fst(size_t len, float* output, const float* input) {
		int iterations = log2i(len);
		if (iterations < 0) return;

		float invLen = 1.0f / static_cast<float>(len);

		for (size_t i = 0; i < len; i++) {
			for (size_t j = 0; j < iterations; j++) {

			}
		}
	}
}