/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
// sims: triangle rasterizer, see TriangleRaster.h
#include "world/utils/TriangleRaster.h"
#include "world/WorldState.h"
#include "world/utils/_2DWorldBatch.h"
#include <algorithm>
#include <cmath>

namespace FSO { namespace LotView { namespace Utils {

st::Vector2 TileProjectionOrigin(WorldState& world) {
    auto& ws = world.WorldSpace;
    st::Vector3 corner(0, 0, 0);
    switch (world.Rotation()) {
        case WorldRotation::TopLeft: corner = st::Vector3(0, 0, 0); break;
        case WorldRotation::TopRight: corner = st::Vector3(0, 1, 0); break;
        case WorldRotation::BottomRight: corner = st::Vector3(1, 1, 0); break;
        case WorldRotation::BottomLeft: corner = st::Vector3(1, 0, 0); break;
    }
    auto cs = ws.GetScreenFromTile(corner);
    auto scroll = world._2D->GetScroll();
    return st::Vector2(std::floor(ws.CadgeWidth / 2) - cs.X - scroll.X, ws.CadgeBaseLine - ws.TilePxHeight / 2 - cs.Y - scroll.Y);
}

RasterTexture RasterTexture::Half() const {
    RasterTexture r;
    if (!Valid() || Width < 2 || Height < 2) return *this;
    r.Width = Width / 2; r.Height = Height / 2;
    r.Pixels.resize((size_t)(r.Width * r.Height));
    for (int y = 0; y < r.Height; y++) for (int x = 0; x < r.Width; x++) {
        uint32_t s[4] = { Pixels[(size_t)((2 * y) * Width + 2 * x)], Pixels[(size_t)((2 * y) * Width + 2 * x + 1)],
                          Pixels[(size_t)((2 * y + 1) * Width + 2 * x)], Pixels[(size_t)((2 * y + 1) * Width + 2 * x + 1)] };
        uint32_t out = 0;
        for (int ch = 0; ch < 4; ch++) {
            uint32_t sum = 0;
            for (int i = 0; i < 4; i++) sum += (s[i] >> (ch * 8)) & 0xFF;
            out |= ((sum + 2) / 4) << (ch * 8);
        }
        r.Pixels[(size_t)(y * r.Width + x)] = out;
    }
    r.Average = Average;
    return r;
}

void DrawTriangle(Framebuffer& fb, const RasterVertex& a, const RasterVertex& b0, const RasterVertex& c0,
                  const RasterTexture* tex, uint32_t flatColor, float depthBias, RasterStats* stats, uint16_t room) {
    const RasterVertex* B = &b0; const RasterVertex* C = &c0;
    float area = (B->x - a.x) * (C->y - a.y) - (B->y - a.y) * (C->x - a.x);
    if (std::fabs(area) < 1e-6f) return;
    if (area < 0) { std::swap(B, C); area = -area; }
    const RasterVertex& b = *B; const RasterVertex& c = *C;

    int32_t minX = std::max(std::max<int32_t>(0, fb.ClipX0), (int32_t)std::floor(std::min(a.x, std::min(b.x, c.x))));
    int32_t maxX = std::min(std::min(fb.Width, fb.ClipX1) - 1, (int32_t)std::ceil(std::max(a.x, std::max(b.x, c.x))));
    int32_t minY = std::max(std::max<int32_t>(0, fb.ClipY0), (int32_t)std::floor(std::min(a.y, std::min(b.y, c.y))));
    int32_t maxY = std::min(std::min(fb.Height, fb.ClipY1) - 1, (int32_t)std::ceil(std::max(a.y, std::max(b.y, c.y))));
    if (minX > maxX || minY > maxY) return;
    if (stats) stats->Triangles++;

    // attribute planes: A(x,y) = A0 + dAx * (x - a.x) + dAy * (y - a.y)
    float inv = 1.0f / area;
    float e1x = b.x - a.x, e1y = b.y - a.y, e2x = c.x - a.x, e2y = c.y - a.y;
    auto plane = [&](float va, float vb, float vc, float& dx, float& dy) {
        float d1 = vb - va, d2 = vc - va;
        dx = (d1 * e2y - d2 * e1y) * inv;
        dy = (d2 * e1x - d1 * e2x) * inv;
    };
    float zdx, zdy, udx, udy, vdx, vdy, rdx, rdy, gdx, gdy, bdx, bdy;
    plane(a.z, b.z, c.z, zdx, zdy);
    plane(a.u, b.u, c.u, udx, udy);
    plane(a.v, b.v, c.v, vdx, vdy);
    plane(a.r, b.r, c.r, rdx, rdy);
    plane(a.g, b.g, c.g, gdx, gdy);
    plane(a.b, b.b, c.b, bdx, bdy);

    // edge functions (inside when all >= 0 with CCW-in-screen orientation as set up above)
    auto edge = [](const RasterVertex& p, const RasterVertex& q, float x, float y) {
        return (q.x - p.x) * (y - p.y) - (q.y - p.y) * (x - p.x);
    };
    float ex0 = -(c.y - b.y), ey0 = (c.x - b.x);  // d/dx, d/dy of edge(b,c)
    float ex1 = -(a.y - c.y), ey1 = (a.x - c.x);
    float ex2 = -(b.y - a.y), ey2 = (b.x - a.x);

    const bool hasTex = tex != nullptr && tex->Valid();
    const int32_t tw = hasTex ? tex->Width : 1, th = hasTex ? tex->Height : 1;
    const int32_t tmx = tw - 1, tmy = th - 1;
    const uint32_t* tp = hasTex ? tex->Pixels.data() : nullptr;
    int32_t drawn = 0;

    for (int32_t y = minY; y <= maxY; y++) {
        float py = y + 0.5f, px0 = minX + 0.5f;
        float w0 = edge(b, c, px0, py), w1 = edge(c, a, px0, py), w2 = edge(a, b, px0, py);
        float dxp = px0 - a.x, dyp = py - a.y;
        float z = a.z + depthBias + zdx * dxp + zdy * dyp;
        float u = a.u + udx * dxp + udy * dyp, v = a.v + vdx * dxp + vdy * dyp;
        float lr = a.r + rdx * dxp + rdy * dyp, lg = a.g + gdx * dxp + gdy * dyp, lb = a.b + bdx * dxp + bdy * dyp;
        uint32_t* crow = &fb.Color[(size_t)y * fb.Width];
        float* drow = &fb.Depth[(size_t)y * fb.Width];
        uint16_t* rrow = fb.Room.empty() ? nullptr : &fb.Room[(size_t)y * fb.Width];
        for (int32_t x = minX; x <= maxX; x++) {
            if (w0 >= 0 && w1 >= 0 && w2 >= 0 && z <= drow[x]) {
                uint32_t t = hasTex ? tp[(((int32_t)std::floor(v)) & tmy) * tw + (((int32_t)std::floor(u)) & tmx)] : flatColor;
                int32_t r = (int32_t)((t & 0xFF) * lr), g = (int32_t)(((t >> 8) & 0xFF) * lg), bb = (int32_t)(((t >> 16) & 0xFF) * lb);
                crow[x] = Framebuffer::Pack((uint8_t)std::min<int32_t>(255, std::max<int32_t>(0, r)), (uint8_t)std::min<int32_t>(255, std::max<int32_t>(0, g)),
                                            (uint8_t)std::min<int32_t>(255, std::max<int32_t>(0, bb)), 255);
                drow[x] = z;
                if (rrow) rrow[x] = room;
                drawn++;
            }
            w0 += ex0; w1 += ex1; w2 += ex2;
            z += zdx; u += udx; v += vdx; lr += rdx; lg += gdx; lb += bdx;
        }
        (void)ey0; (void)ey1; (void)ey2;
    }
    if (stats) stats->Pixels += drawn;
}

}}}
