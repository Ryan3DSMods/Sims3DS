/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
// XNA math used by Vitaboy: Quaternion and the 4x4 Matrix (Microsoft.Xna.Framework).
// Row-vector convention exactly like XNA: v' = v * M, translation in M41..M43,
// (A * B) applies A first. Kept in FSO::Vitaboy so it cannot clash with st::Matrix
// (the 2x2 subset in core/math.h used by the VM).
#pragma once
#include <cmath>
#include "core/math.h"

namespace FSO { namespace Vitaboy {

using st::Vector2;
using st::Vector3;

struct Quaternion {
    float X = 0, Y = 0, Z = 0, W = 1;
    Quaternion() {}
    Quaternion(float x, float y, float z, float w) : X(x), Y(y), Z(z), W(w) {}
    static Quaternion Identity() { return Quaternion(0, 0, 0, 1); }

    /// XNA Quaternion.Slerp (same branch structure and epsilon).
    static Quaternion Slerp(const Quaternion& q1, const Quaternion& q2, float amount) {
        float num2, num3;
        float num = amount;
        float num4 = q1.X * q2.X + q1.Y * q2.Y + q1.Z * q2.Z + q1.W * q2.W;
        bool flag = false;
        if (num4 < 0.0f) { flag = true; num4 = -num4; }
        if (num4 > 0.999999f) {
            num3 = 1.0f - num;
            num2 = flag ? -num : num;
        } else {
            float num5 = std::acos(num4);
            float num6 = 1.0f / std::sin(num5);
            num3 = std::sin((1.0f - num) * num5) * num6;
            num2 = flag ? (-std::sin(num * num5) * num6) : (std::sin(num * num5) * num6);
        }
        return Quaternion(num3 * q1.X + num2 * q2.X, num3 * q1.Y + num2 * q2.Y,
                          num3 * q1.Z + num2 * q2.Z, num3 * q1.W + num2 * q2.W);
    }
};

struct Matrix {
    float M11 = 1, M12 = 0, M13 = 0, M14 = 0;
    float M21 = 0, M22 = 1, M23 = 0, M24 = 0;
    float M31 = 0, M32 = 0, M33 = 1, M34 = 0;
    float M41 = 0, M42 = 0, M43 = 0, M44 = 1;

    static Matrix Identity() { return Matrix(); }

    static Matrix CreateTranslation(const Vector3& v) { return CreateTranslation(v.X, v.Y, v.Z); }
    static Matrix CreateTranslation(float x, float y, float z) {
        Matrix m; m.M41 = x; m.M42 = y; m.M43 = z; return m;
    }

    static Matrix CreateFromQuaternion(const Quaternion& q) {
        Matrix m;
        float xx = q.X * q.X, yy = q.Y * q.Y, zz = q.Z * q.Z;
        float xy = q.X * q.Y, zw = q.Z * q.W, zx = q.Z * q.X;
        float yw = q.Y * q.W, yz = q.Y * q.Z, xw = q.X * q.W;
        m.M11 = 1.0f - 2.0f * (yy + zz); m.M12 = 2.0f * (xy + zw); m.M13 = 2.0f * (zx - yw);
        m.M21 = 2.0f * (xy - zw); m.M22 = 1.0f - 2.0f * (zz + xx); m.M23 = 2.0f * (yz + xw);
        m.M31 = 2.0f * (zx + yw); m.M32 = 2.0f * (yz - xw); m.M33 = 1.0f - 2.0f * (yy + xx);
        return m;
    }

    static Matrix CreateRotationX(float radians) {
        Matrix m; float c = std::cos(radians), s = std::sin(radians);
        m.M22 = c; m.M23 = s; m.M32 = -s; m.M33 = c; return m;
    }
    static Matrix CreateRotationY(float radians) {
        Matrix m; float c = std::cos(radians), s = std::sin(radians);
        m.M11 = c; m.M13 = -s; m.M31 = s; m.M33 = c; return m;
    }
    static Matrix CreateRotationZ(float radians) {
        Matrix m; float c = std::cos(radians), s = std::sin(radians);
        m.M11 = c; m.M12 = s; m.M21 = -s; m.M22 = c; return m;
    }

    /// XNA Matrix.CreateOrthographicOffCenter.
    static Matrix CreateOrthographicOffCenter(float left, float right, float bottom, float top, float zNearPlane, float zFarPlane) {
        Matrix m;
        m.M11 = 2.0f / (right - left);
        m.M22 = 2.0f / (top - bottom);
        m.M33 = 1.0f / (zNearPlane - zFarPlane);
        m.M41 = (left + right) / (left - right);
        m.M42 = (top + bottom) / (bottom - top);
        m.M43 = zNearPlane / (zNearPlane - zFarPlane);
        m.M44 = 1.0f;
        return m;
    }

    Matrix operator*(const Matrix& b) const {
        const Matrix& a = *this;
        Matrix r;
        r.M11 = a.M11 * b.M11 + a.M12 * b.M21 + a.M13 * b.M31 + a.M14 * b.M41;
        r.M12 = a.M11 * b.M12 + a.M12 * b.M22 + a.M13 * b.M32 + a.M14 * b.M42;
        r.M13 = a.M11 * b.M13 + a.M12 * b.M23 + a.M13 * b.M33 + a.M14 * b.M43;
        r.M14 = a.M11 * b.M14 + a.M12 * b.M24 + a.M13 * b.M34 + a.M14 * b.M44;
        r.M21 = a.M21 * b.M11 + a.M22 * b.M21 + a.M23 * b.M31 + a.M24 * b.M41;
        r.M22 = a.M21 * b.M12 + a.M22 * b.M22 + a.M23 * b.M32 + a.M24 * b.M42;
        r.M23 = a.M21 * b.M13 + a.M22 * b.M23 + a.M23 * b.M33 + a.M24 * b.M43;
        r.M24 = a.M21 * b.M14 + a.M22 * b.M24 + a.M23 * b.M34 + a.M24 * b.M44;
        r.M31 = a.M31 * b.M11 + a.M32 * b.M21 + a.M33 * b.M31 + a.M34 * b.M41;
        r.M32 = a.M31 * b.M12 + a.M32 * b.M22 + a.M33 * b.M32 + a.M34 * b.M42;
        r.M33 = a.M31 * b.M13 + a.M32 * b.M23 + a.M33 * b.M33 + a.M34 * b.M43;
        r.M34 = a.M31 * b.M14 + a.M32 * b.M24 + a.M33 * b.M34 + a.M34 * b.M44;
        r.M41 = a.M41 * b.M11 + a.M42 * b.M21 + a.M43 * b.M31 + a.M44 * b.M41;
        r.M42 = a.M41 * b.M12 + a.M42 * b.M22 + a.M43 * b.M32 + a.M44 * b.M42;
        r.M43 = a.M41 * b.M13 + a.M42 * b.M23 + a.M43 * b.M33 + a.M44 * b.M43;
        r.M44 = a.M41 * b.M14 + a.M42 * b.M24 + a.M43 * b.M34 + a.M44 * b.M44;
        return r;
    }
    Matrix& operator*=(const Matrix& b) { *this = *this * b; return *this; }
};

/// XNA Vector3.Transform(position, matrix) (affine part; w is assumed 1 and the result not divided).
inline Vector3 Transform(const Vector3& v, const Matrix& m) {
    return Vector3(v.X * m.M11 + v.Y * m.M21 + v.Z * m.M31 + m.M41,
                   v.X * m.M12 + v.Y * m.M22 + v.Z * m.M32 + m.M42,
                   v.X * m.M13 + v.Y * m.M23 + v.Z * m.M33 + m.M43);
}

namespace MathHelper {
    inline float ToRadians(float degrees) { return degrees * (float)(M_PI / 180.0); }
}

}}
