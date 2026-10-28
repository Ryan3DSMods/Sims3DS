/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
// AvatarRenderer - software rasterizer for Vitaboy avatars (see AvatarRenderer.h)
#include "vitaboy/AvatarRenderer.h"
#include <algorithm>
#include <cmath>
#include <vector>

namespace FSO { namespace Vitaboy {

using FSO::LotView::WorldRotation;

// ---------------------------------------------------------------- WorldCamera

static float RotationYDegrees(WorldRotation r) {
    switch (r) {
        case WorldRotation::TopLeft: return 315.0f;
        case WorldRotation::TopRight: return 225.0f;
        case WorldRotation::BottomRight: return 135.0f;
        case WorldRotation::BottomLeft: return 45.0f;
    }
    return 0.0f;
}

Matrix WorldCamera::Projection() const {
    float diagnal = 64.0f;
    float depthRange = 256.0f;
    if (Zoom == WorldZoom::Medium) {
        diagnal = 128.0f;
        depthRange = 128.0f;
    } else if (Zoom == WorldZoom::Near) {
        diagnal = 256.0f;
        depthRange = 64.0f;
    }
    diagnal *= PreciseZoom;
    depthRange /= PreciseZoom;

    double isoScale = std::sqrt(WorldUnitsPerTile * WorldUnitsPerTile * 2.0f) / diagnal;
    // C#: hb/vb come from the device viewport, hb2/vb2 from ViewDimensions; here they are the same surface.
    auto hb = (float)(ViewDimensions.X * isoScale);
    auto vb = (float)(ViewDimensions.Y * isoScale);
    auto hb2 = (float)(ViewDimensions.X * isoScale);
    auto vb2 = (float)(ViewDimensions.Y * isoScale);

    return Matrix::CreateOrthographicOffCenter(-hb2, -hb2 + (hb * 2), vb2 - (vb * 2), vb2, -(150.0f + depthRange - 64), depthRange);
}

Matrix WorldCamera::GetRotationMatrix() const {
    auto view = Matrix::Identity();
    view *= Matrix::CreateRotationY(MathHelper::ToRadians(RotationYDegrees(Rotation)));
    view *= Matrix::CreateRotationX(MathHelper::ToRadians(30.0f));
    return view;
}

Matrix WorldCamera::View() const {
    auto centerX = CenterTile.X * WorldUnitsPerTile;
    auto centerY = CenterTile.Y * WorldUnitsPerTile;
    auto offset = Vector3(-centerX, 0.0f, -centerY);
    auto view = Matrix::Identity();
    view *= Matrix::CreateTranslation(offset.X, offset.Y, offset.Z);
    view *= Matrix::CreateRotationY(MathHelper::ToRadians(RotationYDegrees(Rotation)));
    view *= Matrix::CreateRotationX(MathHelper::ToRadians(30.0f));
    return view;
}

Matrix WorldCamera::ViewProjectionViewport() const {
    Matrix vp;
    vp.M11 = ViewDimensions.X / 2;
    vp.M22 = -ViewDimensions.Y / 2;
    vp.M41 = ViewDimensions.X / 2 + PixelOffset.X;
    vp.M42 = ViewDimensions.Y / 2 + PixelOffset.Y;
    return View() * Projection() * vp;
}

Vector3 WorldCamera::ProjectWorld(const Vector3& world) const {
    return Transform(world, ViewProjectionViewport());
}

float WorldCamera::TilePxWidth() const {
    switch (Zoom) {
        case WorldZoom::Far: return 32.0f * PreciseZoom;
        case WorldZoom::Medium: return 64.0f * PreciseZoom;
        case WorldZoom::Near: return 128.0f * PreciseZoom;
    }
    return 128.0f;
}

Vector2 WorldCamera::ScreenFromTile(const Vector3& tile) const {
    float TilePxWidthHalf = TilePxWidth() / 2, TilePxHeightHalf = TilePxWidth() / 4;
    float OneUnitDistance = (float)std::sqrt(std::pow(TilePxWidth(), 2) / 2.0);
    float zf = tile.Z * OneUnitDistance * (float)std::cos(M_PI / 6);
    float screenx = 0, screeny = 0;
    switch (Rotation) {
        case WorldRotation::TopLeft:
            screenx = ((tile.X - tile.Y) * TilePxWidthHalf);
            screeny = ((tile.X + tile.Y) * TilePxHeightHalf) - zf;
            break;
        case WorldRotation::TopRight:
            screenx = ((-tile.X - tile.Y) * TilePxWidthHalf);
            screeny = ((tile.X - tile.Y) * TilePxHeightHalf) - zf;
            break;
        case WorldRotation::BottomRight:
            screenx = ((-tile.X + tile.Y) * TilePxWidthHalf);
            screeny = ((-tile.X - tile.Y) * TilePxHeightHalf) - zf;
            break;
        case WorldRotation::BottomLeft:
            screenx = ((tile.X + tile.Y) * TilePxWidthHalf);
            screeny = ((-tile.X + tile.Y) * TilePxHeightHalf) - zf;
            break;
    }
    return Vector2(screenx, screeny);
}

// ---------------------------------------------------------------- rasterizer

namespace {

struct ScreenVert { float x, y, z; };

struct RasterTarget {
    Framebuffer* fb;
    ZBuffer* zb;
    int32_t cx0, cy0, cx1, cy1; // clip, exclusive max
    const Texture* tex;
    uint32_t tintR, tintG, tintB, tintA; // 0..256
    bool bilinear, depthTest, depthWrite;
    AvatarCullMode cull;
    AvatarDrawResult* stats;
    float zLo = 0.0f, zHi = 1.0f; // clip range of the stored depth
};

void RasterTriangle(RasterTarget& t, const ScreenVert& a0, const ScreenVert& b0, const ScreenVert& c0,
                    const Vector2& ta0, const Vector2& tb0, const Vector2& tc0) {
    // signed area in screen space (y down). Positive = clockwise on screen.
    float area = (b0.x - a0.x) * (c0.y - a0.y) - (b0.y - a0.y) * (c0.x - a0.x);
    if (area == 0) return;
    if (t.cull == AvatarCullMode::Back && area < 0) return;
    if (t.cull == AvatarCullMode::Front && area > 0) return;
    const ScreenVert *a = &a0, *b = &b0, *c = &c0;
    const Vector2 *ta = &ta0, *tb = &tb0, *tc = &tc0;
    if (area < 0) { std::swap(b, c); std::swap(tb, tc); area = -area; }

    // FSO_CPP (3DS): tight pixel-centre bounds (x + 0.5 inside [min, max]) with a cheap floor; most avatar triangles
    // are a few pixels, so triangles that cover no pixel centre are rejected before any setup.
    auto ffloor = [](float v) { return (int32_t)(v + 32768.0f) - 32768; };
    float fminx = std::min(a->x, std::min(b->x, c->x)), fmaxx = std::max(a->x, std::max(b->x, c->x));
    float fminy = std::min(a->y, std::min(b->y, c->y)), fmaxy = std::max(a->y, std::max(b->y, c->y));
    int32_t minx = std::max(t.cx0, -ffloor(0.5f - fminx));
    int32_t maxx = std::min(t.cx1 - 1, ffloor(fmaxx - 0.5f));
    int32_t miny = std::max(t.cy0, -ffloor(0.5f - fminy));
    int32_t maxy = std::min(t.cy1 - 1, ffloor(fmaxy - 0.5f));
    if (minx > maxx || miny > maxy) return;
    t.stats->TrianglesDrawn++;

    float inv = 1.0f / area;
    // edge functions E(p) = (q1.x-q0.x)*(p.y-q0.y) - (q1.y-q0.y)*(p.x-q0.x), >= 0 inside
    float e0dx = -(c->y - b->y), e0dy = (c->x - b->x); // opposite a
    float e1dx = -(a->y - c->y), e1dy = (a->x - c->x); // opposite b
    float e2dx = -(b->y - a->y), e2dy = (b->x - a->x); // opposite c
    float px = minx + 0.5f, py = miny + 0.5f;
    float e0row = (c->x - b->x) * (py - b->y) - (c->y - b->y) * (px - b->x);
    float e1row = (a->x - c->x) * (py - c->y) - (a->y - c->y) * (px - c->x);
    float e2row = (b->x - a->x) * (py - a->y) - (b->y - a->y) * (px - a->x);
    // inclusive edges: pixels on a shared edge may be written twice (same texel, depth-tested), no gaps

    // FSO_CPP (3DS): attributes as planes stepped per pixel (was 3 barycentric multiplies per attribute per pixel);
    // texture coordinates pre-scaled to texels.
    const Texture* tex = t.tex;
    const bool fastTex = tex != nullptr && !t.bilinear && tex->WidthMask && tex->HeightMask;
    float tw = fastTex ? (float)tex->Width : 1.0f, th = fastTex ? (float)tex->Height : 1.0f;
    // w0 = e0*inv weights a, w1 = e1*inv weights b, w2 = 1 - w0 - w1 weights c:
    // A = c + (A_a - A_c) * w0 + (A_b - A_c) * w1
    auto grad = [&](float va, float vb, float vc, float& dx, float& dy, float& row) {
        float ka = (va - vc) * inv, kb = (vb - vc) * inv;
        dx = ka * e0dx + kb * e1dx;
        dy = ka * e0dy + kb * e1dy;
        row = vc + ka * e0row + kb * e1row;
    };
    float zdx, zdy, zrow, udx, udy, urow, vdx, vdy, vrow;
    grad(a->z, b->z, c->z, zdx, zdy, zrow);
    grad(ta->X * tw, tb->X * tw, tc->X * tw, udx, udy, urow);
    grad(ta->Y * th, tb->Y * th, tc->Y * th, vdx, vdy, vrow);

    float* zbase = (t.zb && t.zb->Depth) ? t.zb->Depth : nullptr;
    const int32_t zstride = zbase ? t.zb->Stride : 0;
    const bool depthTest = zbase && t.depthTest, depthWrite = zbase && t.depthWrite;
    uint8_t* const pbase = t.fb->Pixels;
    const int32_t pstride = t.fb->Stride;
    const uint32_t tr = t.tintR, tg = t.tintG, tb2 = t.tintB, ta2 = t.tintA;
    int32_t drawn = 0;

    for (int32_t y = miny; y <= maxy; y++) {
        float e0 = e0row, e1 = e1row, e2 = e2row;
        float z = zrow, u = urow, v = vrow;
        float* zp = zbase ? zbase + y * zstride : nullptr;
        uint8_t* prow = pbase + y * pstride;
        for (int32_t x = minx; x <= maxx; x++) {
            if (e0 >= 0 && e1 >= 0 && e2 >= 0 && z >= t.zLo && z <= t.zHi && !(depthTest && zp[x] < z)) {
                uint32_t col;
                if (fastTex) {
                    int32_t ix = ((int32_t)(u + 65536.0f)) & tex->WidthMask, iy = ((int32_t)(v + 65536.0f)) & tex->HeightMask;
                    int32_t i = iy * tex->Width + ix;
                    col = tex->Paletted ? tex->Palette[tex->Indices[i]] : tex->Pixels[i];
                } else if (tex) col = t.bilinear ? tex->SampleBilinear(u / tw, v / th) : tex->SampleNearest(u / tw, v / th);
                else col = 0xFF808080u;
                uint8_t* p = prow + x * 4;
                p[0] = (uint8_t)(((col & 255) * tr) >> 8);
                p[1] = (uint8_t)((((col >> 8) & 255) * tg) >> 8);
                p[2] = (uint8_t)((((col >> 16) & 255) * tb2) >> 8);
                p[3] = (uint8_t)((((col >> 24) & 255) * ta2) >> 8);
                if (depthWrite) zp[x] = z;
                drawn++;
            }
            e0 += e0dx; e1 += e1dx; e2 += e2dx;
            z += zdx; u += udx; v += vdx;
        }
        e0row += e0dy; e1row += e1dy; e2row += e2dy;
        zrow += zdy; urow += udy; vrow += vdy;
    }
    t.stats->Pixels += drawn;
}

} // namespace

AvatarDrawResult DrawAvatar(Framebuffer& color, ZBuffer& depth, const WorldCamera& camera, Avatar& avatar,
                            const Vector3& tilePosition, float radianDirection, const AvatarDrawOptions& options) {
    AvatarDrawResult result;
    if (color.Pixels == nullptr || color.Width <= 0 || color.Height <= 0) return result;
    auto& skelBones = avatar.SkelBones();
    if (skelBones.empty()) return result;

    // AvatarComponent.Draw: world._3D.DrawMesh(Matrix.CreateRotationY((float)(Math.PI-RadianDirection))*this.World, ...)
    // EntityComponent.World: Matrix.CreateTranslation(WorldSpace.GetWorldFromTile(Position))
    Matrix world = Matrix::CreateRotationY((float)(M_PI - radianDirection)) *
                   Matrix::CreateTranslation(WorldCamera::GetWorldFromTile(tilePosition));
    Matrix wvp = world * camera.ViewProjectionViewport();

    // vsVitaboy: position = (1-w)*mul(pos, SkelBindings[x]) + w*mul(bv, SkelBindings[y]); then World*View*Projection.
    // The camera is orthographic (affine), so fold each bone matrix with the whole transform.
    std::vector<Matrix> boneToScreen(skelBones.size());
    for (size_t i = 0; i < skelBones.size(); i++) boneToScreen[i] = skelBones[i] * wvp;

    RasterTarget t;
    t.fb = &color;
    t.zb = (depth.Depth != nullptr && depth.Width >= color.Width && depth.Height >= color.Height) ? &depth : nullptr;
    t.cx0 = 0; t.cy0 = 0; t.cx1 = color.Width; t.cy1 = color.Height;
    if (options.Clip.Width > 0 && options.Clip.Height > 0) {
        t.cx0 = std::max(t.cx0, options.Clip.X); t.cy0 = std::max(t.cy0, options.Clip.Y);
        t.cx1 = std::min(t.cx1, options.Clip.X + options.Clip.Width); t.cy1 = std::min(t.cy1, options.Clip.Y + options.Clip.Height);
    }
    t.tintR = options.Tint.R + (options.Tint.R >> 7); // 255 -> 256
    t.tintG = options.Tint.G + (options.Tint.G >> 7);
    t.tintB = options.Tint.B + (options.Tint.B >> 7);
    t.tintA = options.Tint.A + (options.Tint.A >> 7);
    t.bilinear = options.Bilinear;
    t.depthTest = options.DepthTest;
    t.depthWrite = options.DepthWrite;
    t.cull = options.Cull;
    t.stats = &result;
    const float dS = options.DepthScale, dO = options.DepthOffset;
    t.zLo = std::min(dO, dS + dO); t.zHi = std::max(dO, dS + dO);

    float minX = 1e30f, minY = 1e30f, maxX = -1e30f, maxY = -1e30f;
    std::vector<ScreenVert> sv;
    int32_t nbones = (int32_t)boneToScreen.size();
    for (auto& binding : avatar.Bindings) {
        Mesh* mesh = binding->Mesh;
        if (mesh == nullptr) continue;
        t.tex = binding->Texture;
        auto& vb = mesh->VertexBuffer;
        sv.resize(vb.size());
        for (size_t i = 0; i < vb.size(); i++) {
            auto& v = vb[i];
            int32_t bx = (int32_t)v.Parameters.X;
            if (bx < 0 || bx >= nbones) bx = 0;
            Vector3 p = Transform(v.Position, boneToScreen[bx]);
            float w = v.Parameters.Z;
            if (w != 0) {
                int32_t by = (int32_t)v.Parameters.Y;
                if (by < 0 || by >= nbones) by = 0;
                Vector3 q = Transform(v.BvPosition, boneToScreen[by]);
                p = p * (1.0f - w) + q * w;
            }
            sv[i] = ScreenVert{ p.X, p.Y, p.Z * dS + dO };
            minX = std::min(minX, p.X); maxX = std::max(maxX, p.X);
            minY = std::min(minY, p.Y); maxY = std::max(maxY, p.Y);
        }
        auto& ib = mesh->IndexBuffer;
        int32_t nv = (int32_t)vb.size();
        for (size_t f = 0; f + 2 < ib.size(); f += 3) {
            int32_t i0 = ib[f], i1 = ib[f + 1], i2 = ib[f + 2];
            if (i0 < 0 || i1 < 0 || i2 < 0 || i0 >= nv || i1 >= nv || i2 >= nv) continue;
            result.Triangles++;
            RasterTriangle(t, sv[i0], sv[i1], sv[i2], vb[i0].TextureCoordinate, vb[i1].TextureCoordinate, vb[i2].TextureCoordinate);
        }
    }
    if (maxX >= minX) {
        result.MinX = (int32_t)std::floor(minX); result.MinY = (int32_t)std::floor(minY);
        result.MaxX = (int32_t)std::ceil(maxX); result.MaxY = (int32_t)std::ceil(maxY);
    }
    return result;
}

}}
