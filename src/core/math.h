/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
// Small math structs standing in for the XNA types used by the FreeSims engine
// (Microsoft.Xna.Framework.Vector2/Vector3/Point/Rectangle/Matrix/Color).
#pragma once
#include <cstdint>
#include <cmath>
#include <algorithm>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace st {

struct Vector2 {
    float X = 0, Y = 0;
    Vector2() {}
    Vector2(float x, float y) : X(x), Y(y) {}
    Vector2 operator+(const Vector2& o) const { return Vector2(X + o.X, Y + o.Y); }
    Vector2 operator-(const Vector2& o) const { return Vector2(X - o.X, Y - o.Y); }
    Vector2 operator*(float f) const { return Vector2(X * f, Y * f); }
    Vector2 operator/(float f) const { return Vector2(X / f, Y / f); }
    Vector2& operator+=(const Vector2& o) { X += o.X; Y += o.Y; return *this; }
    Vector2& operator-=(const Vector2& o) { X -= o.X; Y -= o.Y; return *this; }
    Vector2& operator/=(float f) { X /= f; Y /= f; return *this; }
    bool operator==(const Vector2& o) const { return X == o.X && Y == o.Y; }
    bool operator!=(const Vector2& o) const { return !(*this == o); }
};

struct Vector3 {
    float X = 0, Y = 0, Z = 0;
    Vector3() {}
    Vector3(float x, float y, float z) : X(x), Y(y), Z(z) {}
    Vector3 operator+(const Vector3& o) const { return Vector3(X + o.X, Y + o.Y, Z + o.Z); }
    Vector3 operator-(const Vector3& o) const { return Vector3(X - o.X, Y - o.Y, Z - o.Z); }
    Vector3 operator*(float f) const { return Vector3(X * f, Y * f, Z * f); }
    Vector3& operator+=(const Vector3& o) { X += o.X; Y += o.Y; Z += o.Z; return *this; }
    bool operator==(const Vector3& o) const { return X == o.X && Y == o.Y && Z == o.Z; }
    bool operator!=(const Vector3& o) const { return !(*this == o); }
    static Vector3 Lerp(const Vector3& a, const Vector3& b, float t) {
        return Vector3(a.X + (b.X - a.X) * t, a.Y + (b.Y - a.Y) * t, a.Z + (b.Z - a.Z) * t);
    }
};
inline Vector3 operator*(float f, const Vector3& v) { return v * f; }

struct Point {
    int32_t X = 0, Y = 0;
    Point() {}
    Point(int32_t x, int32_t y) : X(x), Y(y) {}
    Point operator+(const Point& o) const { return Point(X + o.X, Y + o.Y); }
    Point operator-(const Point& o) const { return Point(X - o.X, Y - o.Y); }
    Point& operator+=(const Point& o) { X += o.X; Y += o.Y; return *this; }
    bool operator==(const Point& o) const { return X == o.X && Y == o.Y; }
    bool operator!=(const Point& o) const { return !(*this == o); }
    bool Equals(const Point& o) const { return *this == o; }
};

struct Rectangle {
    int32_t X = 0, Y = 0, Width = 0, Height = 0;
    Rectangle() {}
    Rectangle(int32_t x, int32_t y, int32_t w, int32_t h) : X(x), Y(y), Width(w), Height(h) {}
    int32_t Left() const { return X; }
    int32_t Right() const { return X + Width; }
    int32_t Top() const { return Y; }
    int32_t Bottom() const { return Y + Height; }
    bool Contains(int32_t x, int32_t y) const { return x >= X && x < X + Width && y >= Y && y < Y + Height; }
};

// Only the subset of Matrix needed by the engine: Z rotation applied to a Vector3.
struct Matrix {
    float M11 = 1, M12 = 0, M21 = 0, M22 = 1;
    static Matrix CreateRotationZ(float radians) {
        Matrix m;
        float c = std::cos(radians), s = std::sin(radians);
        m.M11 = c; m.M12 = s; m.M21 = -s; m.M22 = c;
        return m;
    }
};
// XNA Vector3.Transform(v, m) for a Z rotation matrix (row-vector convention).
inline Vector3 Transform(const Vector3& v, const Matrix& m) {
    return Vector3(v.X * m.M11 + v.Y * m.M21, v.X * m.M12 + v.Y * m.M22, v.Z);
}

// NOTE: XNA default(Color) is (0,0,0,0) i.e. transparent; `new Color[n]` yields transparent entries.
struct Color {
    uint8_t R = 0, G = 0, B = 0, A = 0;
    Color() {}
    Color(int r, int g, int b) : R((uint8_t)std::clamp(r, 0, 255)), G((uint8_t)std::clamp(g, 0, 255)), B((uint8_t)std::clamp(b, 0, 255)), A(255) {}
    Color(int r, int g, int b, int a) : R((uint8_t)std::clamp(r, 0, 255)), G((uint8_t)std::clamp(g, 0, 255)), B((uint8_t)std::clamp(b, 0, 255)), A((uint8_t)std::clamp(a, 0, 255)) {}
    Color operator*(float f) const {
        return Color((int)std::clamp(R * f, 0.0f, 255.0f), (int)std::clamp(G * f, 0.0f, 255.0f), (int)std::clamp(B * f, 0.0f, 255.0f), (int)std::clamp(A * f, 0.0f, 255.0f));
    }
    bool operator==(const Color& o) const { return R == o.R && G == o.G && B == o.B && A == o.A; }
    bool operator!=(const Color& o) const { return !(*this == o); }
    uint32_t PackedValue() const { return (uint32_t)R | ((uint32_t)G << 8) | ((uint32_t)B << 16) | ((uint32_t)A << 24); }
    static Color White() { return Color(255, 255, 255, 255); }
    static Color Black() { return Color(0, 0, 0, 255); }
    static Color Transparent() { return Color(0, 0, 0, 0); }
    static Color Lerp(const Color& a, const Color& b, float t) {
        return Color((int)(a.R + (b.R - a.R) * t), (int)(a.G + (b.G - a.G) * t), (int)(a.B + (b.B - a.B) * t), (int)(a.A + (b.A - a.A) * t));
    }
};

} // namespace st
