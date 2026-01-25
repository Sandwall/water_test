#pragma once

template <typename T = float>
union Vector3 {
private:
	float v[3];
public:
	struct {
		float x;
		float y;
		float z;
	};

	float& operator[](int idx) { return v[idx]; }
	const float& operator[](int idx) const { return v[idx]; }

	//Vector3 operator+(const Vector3& rhs) const;
	//Vector3 operator-(const Vector3& rhs) const;
	//Vector3 operator*(const Vector3& rhs) const;

};

struct Matrix4 {
	// column-major: m[col * 4 + row]
	float m[16];

	// constructors
	explicit Matrix4(float diag = 1.0f);

	// element access
	float& operator()(int row, int col);
	const float& operator()(int row, int col) const;

	// arithmetic
	Matrix4 operator+(const Matrix4& rhs) const;
	Matrix4& operator+=(const Matrix4& rhs);

	Matrix4 operator-() const;

	Matrix4 operator*(const Matrix4& rhs) const;
	Matrix4& operator*=(const Matrix4& rhs);

	// static constructors
	static Matrix4 identity();

	static Matrix4 perspective(float fovyRadians,
		float aspect,
		float zNear,
		float zFar);

	static Matrix4 translation(float x, float y, float z);
	static Matrix4 scale(float x, float y, float z);

	static Matrix4 rotationX(float radians);
	static Matrix4 rotationY(float radians);
	static Matrix4 rotationZ(float radians);

	static Matrix4 transform(float tx, float ty, float tz,
		float rx, float ry, float rz,
		float sx, float sy, float sz);5

};

union Complex {
	struct {
		float re, im;
	};

	float& operator[](int i) {
		return data[i];
	}

	const float& operator[](int i) const {
		return data[i];
	}

	void operator=(float val) {
		re = val;
		im = val;
	}

	Complex(float real = 0.0f, float imaginary = 0.0f) {
		re = real;
		im = imaginary;
	}

	// addition
	Complex operator+(const Complex& rhs) const {
		return Complex(re + rhs.re, im + rhs.im);
	}

	Complex& operator+=(const Complex& rhs) {
		re += rhs.re;
		im += rhs.im;
		return *this;
	}

	// subtraction
	Complex operator-(const Complex& rhs) const {
		return Complex(re - rhs.re, im - rhs.im);
	}

	Complex& operator-=(const Complex& rhs) {
		re -= rhs.re;
		im -= rhs.im;
		return *this;
	}

	// multiplication
	Complex operator*(const Complex& rhs) const {
		return Complex(
			re * rhs.re - im * rhs.im,
			re * rhs.im + im * rhs.re
		);
	}

	Complex& operator*=(const Complex& rhs) {
		float r = re * rhs.re - im * rhs.im;
		float i = re * rhs.im + im * rhs.re;
		re = r;
		im = i;
		return *this;
	}

	// division
	Complex operator/(const Complex& rhs) const {
		float denom = rhs.re * rhs.re + rhs.im * rhs.im;
		return Complex(
			(re * rhs.re + im * rhs.im) / denom,
			(im * rhs.re - re * rhs.im) / denom
		);
	}

	Complex& operator/=(const Complex& rhs) {
		float denom = rhs.re * rhs.re + rhs.im * rhs.im;
		float r = (re * rhs.re + im * rhs.im) / denom;
		float i = (im * rhs.re - re * rhs.im) / denom;
		re = r;
		im = i;
		return *this;
	}

private:
	float data[2];
};

namespace smath {
	void dst(size_t len, float* output, const float* input);
	void dct(size_t len, float* output, const float* input);
	void dft(size_t len, Complex* output, const Complex* input);
	
	// cooley-tukey algorithm
	void fst(size_t len, float* output, const float* input);
	//void fct(size_t len, float* output, const float* input);
	void fft(size_t len, Complex* output, const Complex* input);
}