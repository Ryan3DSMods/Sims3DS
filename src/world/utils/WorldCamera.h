/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
// FSO.LotView.Utils.WorldCamera (World/utils/WorldCamera.cs)
//
// The C# camera is an XNA OrthographicCamera whose View/Projection matrices feed the vertex shaders of
// 2DWorldBatch.fx. The 2D renderer only uses it to turn a sprite's world position into a depth value
// (vsZSprite: backProjection.z / backProjection.w). With an orthographic projection that depth is an affine
// function of the view-space z, so on the CPU we keep exactly that: Depth(p) = -viewZ(p), where
//     view = Translate(-center) * RotationY(rotY) * RotationX(30deg)        (CalculateView)
// The projection's depth scale (1/(far-near)) and offset are positive constants for a given zoom and cancel
// out of every depth comparison (sprites are only ever compared with each other within one zoom), as does
// the center translation. Smaller = nearer, like the D3D LessEqual depth test.
#pragma once
#include <cmath>
#include "core/math.h"
#include "world/WorldZoom.h"
#include "world/model/WorldRotation.h"

namespace FSO { namespace LotView { namespace Utils {

class WorldCamera {
public:
    float WorldSize = 0;
    st::Vector2 ViewDimensions = st::Vector2(-1, -1);
    st::Vector2 CenterTile;
    WorldRotation Rotation = WorldRotation::TopLeft;
    float PreciseZoom = 1.0f;
    WorldZoom Zoom = WorldZoom::Near;
    float RotateX = 30.0f;
    float RotateY = 315.0f;

    static float RotationYDegrees(WorldRotation rot) {
        switch (rot) {
            case WorldRotation::TopLeft: return 315.0f;
            case WorldRotation::TopRight: return 225.0f;
            case WorldRotation::BottomRight: return 135.0f;
            case WorldRotation::BottomLeft: return 45.0f;
        }
        return 315.0f;
    }

    /// The linear part of GetRotationMatrix()'s z row: viewZ = x*ZX + y*ZY + z*ZZ (XNA row-vector convention).
    void GetDepthBasis(float& zx, float& zy, float& zz) const {
        const double deg = M_PI / 180.0;
        double ry = RotationYDegrees(Rotation) * deg, rx = 30.0 * deg;
        // RotationY: x' = x cos + z sin ; z' = -x sin + z cos.  RotationX: z'' = y sin(rx) + z' cos(rx)
        zx = (float)(-std::sin(ry) * std::cos(rx));
        zy = (float)(std::sin(rx));
        zz = (float)(std::cos(ry) * std::cos(rx));
    }

    /// Depth of a world-space point (WorldSpace.GetWorldFromTile units: X = tile x*3, Y = height, Z = tile y*3).
    float Depth(const st::Vector3& world) const {
        float zx, zy, zz;
        GetDepthBasis(zx, zy, zz);
        return -(world.X * zx + world.Y * zy + world.Z * zz);
    }
};

}}}
