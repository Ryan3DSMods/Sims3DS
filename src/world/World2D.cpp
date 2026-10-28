/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
// FSO.LotView.World2D (World/World2D.cs)
#include "world/World2D.h"
#include "world/WorldState.h"
#include "world/model/Blueprint.h"
#include "world/utils/_2DWorldBatch.h"
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <unordered_set>
#include <limits>
#include <cstring>
#include <algorithm>

namespace FSO { namespace LotView {

using namespace FSO::LotView::Model;
using namespace FSO::LotView::Components;

static WorldObjectRenderInfo* GetRenderInfo(WorldComponent* component) {
    auto obj = dynamic_cast<ObjectComponent*>(component);
    return (obj == nullptr) ? nullptr : &obj->renderInfo;
}

void World2D::PreDraw(WorldState& state) {
    if (Blueprint == nullptr) return;
    auto pxOffset = state.WorldSpace.GetScreenOffset();
    pxOffset = st::Vector2(-pxOffset.X, -pxOffset.Y);
    auto& damage = Blueprint->Damage;
    auto _2d = state._2D;

    /**
     * Tasks:
     *  If zoom or rotation has changed, redraw all static layers
     *  If scroll has changed, redraw static layer if the scroll is outwith the buffered region
     *  If architecture has changed, redraw appropriate static layer
     *  If there is a new object in the static layer, redraw the static layer
     *  If an objects in the static layer has changed, redraw the static layer and move the object to the dynamic layer
     *  If wall visibility has changed, redraw wall layer (should think about how this works with breakthrough wall mode
     */

    auto redrawStaticObjects = false;
    auto redrawFloor = false;
    auto redrawWall = false;

    auto recacheWalls = false;
    auto recacheFloors = false;
    auto recacheTerrain = false;
    auto recacheObjects = false;
    bool relight = false;
    bool cutChanged = false;
    std::vector<st::Rectangle> objDirty; // FSO_CPP: world-pixel areas of the static object layer to redraw

    if (TicksSinceLight++ > 60 * 4) damage.push_back(BlueprintDamage(BlueprintDamageType::LIGHTING_CHANGED));

    // FSO_CPP: damage can reference components the VM has deleted since (C# keeps them alive via the GC).
    std::unordered_set<WorldComponent*> live(Blueprint->Objects.begin(), Blueprint->Objects.end());

    WorldObjectRenderInfo* info = nullptr;
    for (auto& item : damage) {
        switch (item.Type) {
            case BlueprintDamageType::ROTATE:
            case BlueprintDamageType::ZOOM:
            case BlueprintDamageType::LEVEL_CHANGED:
                recacheObjects = true;
                recacheWalls = true;
                recacheFloors = true;
                recacheTerrain = true;
                break;
            case BlueprintDamageType::SCROLL:
                // FSO_CPP: handled below by shifting the layers (see World2D.h)
                break;
            case BlueprintDamageType::PRECISE_ZOOM:
                redrawFloor = redrawWall = redrawStaticObjects = true;
                break;
            case BlueprintDamageType::LIGHTING_CHANGED: {
                Blueprint->GenerateRoomLights();
                state.OutsideColor = Blueprint->RoomColors[1];
                _2d->AmbientLight = Blueprint->RoomColors.data();
                TicksSinceLight = 0;
                // FSO_CPP: C# re-renders the static layers on every lighting tick (every 4s); on the CPU that is
                // expensive, so only do it when a room colour actually changed.
                bool changed = LastRoomColors.size() != Blueprint->Light.size() + 1;
                if (!changed) {
                    for (size_t i = 0; i < Blueprint->Light.size(); i++)
                        if (LastRoomColors[i] != Blueprint->RoomColors[i].PackedValue()) { changed = true; break; }
                }
                if (state.OutsideColor.PackedValue() != LastOutside) changed = true;
                if (changed) {
                    LastRoomColors.resize(Blueprint->Light.size() + 1);
                    for (size_t i = 0; i < Blueprint->Light.size(); i++) LastRoomColors[i] = Blueprint->RoomColors[i].PackedValue();
                    LastOutside = state.OutsideColor.PackedValue();
                    // FSO_CPP: the static layers hold unlit colour + room ids, only the composite is rebuilt
                    relight = true;
                }
                break;
            }
            case BlueprintDamageType::OBJECT_MOVE:
            case BlueprintDamageType::OBJECT_GRAPHIC_CHANGE: {
                /** Redraw if its in static layer **/
                if (item.Component == nullptr || !live.count(item.Component)) {
                    // removed object: it may still be painted in the static layer
                    // FSO_CPP: only the area it covered is redrawn (C# redrew the whole layer)
                    if (item.Component != nullptr) TakeStaticBounds(item.Component, objDirty, recacheObjects);
                    break;
                }
                info = GetRenderInfo(item.Component);
                if (info != nullptr && info->Layer == WorldObjectRenderLayer::STATIC) {
                    TakeStaticBounds(item.Component, objDirty, recacheObjects);
                    info->Layer = WorldObjectRenderLayer::DYNAMIC;
                }
                if (auto oc = dynamic_cast<ObjectComponent*>(item.Component)) oc->DynamicCounter = 0;
                break;
            }
            case BlueprintDamageType::OBJECT_RETURN_TO_STATIC:
                if (item.Component == nullptr || !live.count(item.Component)) break;
                info = GetRenderInfo(item.Component);
                if (info != nullptr && info->Layer == WorldObjectRenderLayer::DYNAMIC) {
                    info->Layer = WorldObjectRenderLayer::STATIC;
                    if (auto oc = dynamic_cast<ObjectComponent*>(item.Component)) {
                        auto r = ObjectScreenBounds(oc, state);
                        if (r.Width > 0) objDirty.push_back(r); // empty sprite: nothing to paint
                    } else recacheObjects = true;
                }
                break;
            case BlueprintDamageType::WALL_CUT_CHANGED:
                cutChanged = true; // FSO_CPP: resolved after the loop (only the changed walls when possible)
                break;
            case BlueprintDamageType::ROOF_STYLE_CHANGED:
                Blueprint->RoofComp.StyleDirty = true;
                if (state.DrawRoofs) recacheWalls = true; // the roof is part of the wall layer
                break;
            case BlueprintDamageType::ROOM_CHANGED:
                Blueprint->RoofComp.ShapeDirty = true;
                if (state.DrawRoofs) recacheWalls = true;
                break;
            case BlueprintDamageType::FLOOR_CHANGED:
            case BlueprintDamageType::WALL_CHANGED:
                recacheTerrain = true;
                recacheFloors = true;
                recacheWalls = true;
                Blueprint->RoofComp.ShapeDirty = true;
                break;
        }
        if (getenv("WORLD_DAMAGE")) printf("damage %d -> floor %d wall %d obj %d\n", (int)item.Type, (int)(recacheFloors || recacheTerrain), (int)recacheWalls, (int)recacheObjects);
        if (recacheFloors || recacheTerrain) redrawFloor = true;
        if (recacheWalls) redrawWall = true;
        if (recacheObjects) redrawStaticObjects = true;
    }
    damage.clear();
    st::Rectangle cutRect; // buffer-independent world pixels; Width 0 = nothing
    if (cutChanged && !redrawWall) {
        if (!CutDirtyRect(state, cutRect)) redrawWall = true;
        if (getenv("WORLD_DAMAGE")) printf("cut change: rect %d,%d %dx%d full %d\n", cutRect.X, cutRect.Y, cutRect.Width, cutRect.Height, (int)redrawWall);
    }
    if (redrawWall || cutRect.Width > 0) { LastCut = Blueprint->Cutaway; LastCutLevel = state.Level(); }
    // FSO_CPP: World3D drew the roof every frame when State.DrawRoofs; here it lives in the cached wall layer.
    if (state.DrawRoofs != LastDrawRoofs) { LastDrawRoofs = state.DrawRoofs; redrawWall = true; }

    if (_2d->AmbientLight == nullptr) _2d->AmbientLight = Blueprint->RoomColors.data();

    if (recacheTerrain && Blueprint->Terrain)
        Blueprint->Terrain->RegenTerrain(state, Blueprint);

    int32_t bw = (int32_t)state.WorldSpace.WorldPxWidth + SCROLL_BUFFER, bh = (int32_t)state.WorldSpace.WorldPxHeight + SCROLL_BUFFER;
    bool sizeChanged = Composite.Width != bw || Composite.Height != bh;
    auto newOrigin = GetBufferOrigin(pxOffset);
    bool anyInvalid = !StaticFloor.Valid || !StaticWall.Valid || !StaticObjects.Valid || !CompositeValid || !HaveOrigin;
    if (sizeChanged || anyInvalid) {
        redrawFloor = redrawWall = redrawStaticObjects = true;
        HaveOrigin = false;
    }
    bool redraw[3] = { redrawFloor, redrawWall, redrawStaticObjects };
    bool compositeAll = sizeChanged || anyInvalid || redrawFloor || redrawWall || redrawStaticObjects || relight;
    AmbientLight = Blueprint->RoomColors.data();
    OutsideLight = state.OutsideColor;
    if (OutsideLight.A == 0) OutsideLight = st::Color::White();
    if (sizeChanged) Composite.Resize(bw, bh);

    // scrolling: shift the cached pixels, render only what scrolled in
    std::vector<st::Rectangle> strips;
    if (HaveOrigin && newOrigin != Origin) {
        int32_t sx = (int32_t)(Origin.X - newOrigin.X), sy = (int32_t)(Origin.Y - newOrigin.Y);
        if (std::abs(sx) >= bw / 2 || std::abs(sy) >= bh / 2) {
            redraw[0] = redraw[1] = redraw[2] = true;
            compositeAll = true;
        } else {
            for (int l = 0; l < 3; l++) if (!redraw[l]) ShiftBuffer(LayerBuffer(l).Buffer, sx, sy);
            if (!compositeAll) ShiftBuffer(Composite, sx, sy);
            if (sx > 0) strips.push_back(st::Rectangle(0, 0, sx, bh));
            else if (sx < 0) strips.push_back(st::Rectangle(bw + sx, 0, -sx, bh));
            if (sy > 0) strips.push_back(st::Rectangle(0, 0, bw, sy));
            else if (sy < 0) strips.push_back(st::Rectangle(0, bh + sy, bw, -sy));
        }
    }
    Origin = newOrigin;
    HaveOrigin = true;
    for (int l = 0; l < 3; l++) {
        auto& buf = LayerBuffer(l);
        buf.PxOffset = Origin;
        buf.WorldPosition = st::Vector3(state.CenterTile().X, state.CenterTile().Y, 0);
    }

    for (int l = 0; l < 3; l++) {
        if (redraw[l]) { RenderLayer(l, state, nullptr); StaticRedraws++; }
        else for (auto& r : strips) { RenderLayer(l, state, &r); StripRedraws++; StripPixels += (int64_t)r.Width * r.Height; }
    }
    if (compositeAll) BuildComposite(st::Rectangle(0, 0, bw, bh));
    else for (auto& r : strips) BuildComposite(r);
    if (!redraw[WALL] && cutRect.Width > 0) {
        int32_t x0 = std::max<int32_t>(0, cutRect.X - (int32_t)Origin.X), y0 = std::max<int32_t>(0, cutRect.Y - (int32_t)Origin.Y);
        int32_t x1 = std::min<int32_t>(bw, cutRect.X + cutRect.Width - (int32_t)Origin.X), y1 = std::min<int32_t>(bh, cutRect.Y + cutRect.Height - (int32_t)Origin.Y);
        if (x1 > x0 && y1 > y0) {
            st::Rectangle r(x0, y0, x1 - x0, y1 - y0);
            RenderLayer(WALL, state, &r); DirtyRedraws++; StripPixels += (int64_t)r.Width * r.Height;
            if (!compositeAll) BuildComposite(r);
        }
    }
    if (!redraw[OBJECTS] && !objDirty.empty()) {
        int64_t area = 0;
        std::vector<st::Rectangle> rects;
        for (auto& w : objDirty) {
            int32_t x0 = std::max<int32_t>(0, w.X - (int32_t)Origin.X - 2), y0 = std::max<int32_t>(0, w.Y - (int32_t)Origin.Y - 2);
            int32_t x1 = std::min<int32_t>(bw, w.X + w.Width - (int32_t)Origin.X + 2), y1 = std::min<int32_t>(bh, w.Y + w.Height - (int32_t)Origin.Y + 2);
            if (x1 <= x0 || y1 <= y0) continue;
            rects.push_back(st::Rectangle(x0, y0, x1 - x0, y1 - y0));
            area += (int64_t)(x1 - x0) * (y1 - y0);
        }
        if (area * 2 > (int64_t)bw * bh) {
            RenderLayer(OBJECTS, state, nullptr); StaticRedraws++;
            if (!compositeAll) BuildComposite(st::Rectangle(0, 0, bw, bh));
        } else {
            for (auto& r : rects) {
                RenderLayer(OBJECTS, state, &r); DirtyRedraws++; StripPixels += (int64_t)r.Width * r.Height;
                if (!compositeAll) BuildComposite(r);
            }
        }
    }
    CompositeValid = true;
    _2d->Target = nullptr;
}

// FSO_CPP: world-pixel bounds of the walls whose cut changed since the wall layer was rendered. A tile's walls (and
// the junctions/end caps its neighbours draw) lie within the tiles around it, from the floor of the current level up
// to wall height. Returns false when the whole layer should be redrawn (no previous cut, level change, larger than
// the buffer).
bool World2D::CutDirtyRect(WorldState& state, st::Rectangle& out) {
    out = st::Rectangle();
    auto& cut = Blueprint->Cutaway;
    if (LastCut.size() != cut.size() || LastCutLevel != state.Level() || Blueprint->Width <= 0) return false;
    float minX = 1e9f, minY = 1e9f, maxX = -1e9f, maxY = -1e9f;
    int w = Blueprint->Width, n = 0;
    float z0 = (state.Level() - 1) * 2.95f;
    for (size_t i = 0; i < cut.size(); i++) {
        if (cut[i] == LastCut[i]) continue;
        n++;
        int tx = (int)(i % (size_t)w), ty = (int)(i / (size_t)w);
        for (int cx = 0; cx < 2; cx++) for (int cy = 0; cy < 2; cy++) {
            auto p = state.WorldSpace.GetScreenFromTile(st::Vector3((float)(tx - 1 + cx * 3), (float)(ty - 1 + cy * 3), z0));
            minX = std::min(minX, p.X); maxX = std::max(maxX, p.X); minY = std::min(minY, p.Y); maxY = std::max(maxY, p.Y);
        }
    }
    if (n == 0) return true; // nothing visible changed
    // walls rise one story above the floor (FreeSims wall heights: 57 / 115 / 231 px at far / medium / near)
    int32_t wallH = state.Zoom() == WorldZoom::Far ? 57 : (state.Zoom() == WorldZoom::Medium ? 115 : 231);
    int32_t m = (int32_t)(state.WorldSpace.TilePxWidth / 2);
    // same space as ObjectScreenBounds (GetScreenFromTile, buffer pixel = value - Origin)
    out = st::Rectangle((int32_t)std::floor(minX) - m, (int32_t)std::floor(minY) - wallH - m,
                        (int32_t)std::ceil(maxX - minX) + 2 * m, (int32_t)std::ceil(maxY - minY) + wallH + 2 * m);
    int64_t bw = (int64_t)state.WorldSpace.WorldPxWidth + SCROLL_BUFFER, bh = (int64_t)state.WorldSpace.WorldPxHeight + SCROLL_BUFFER;
    return (int64_t)out.Width * out.Height < bw * bh;
}

st::Rectangle World2D::ObjectScreenBounds(ObjectComponent* obj, WorldState& state) {
    obj->ValidateSprite(state);
    auto b = obj->Bounding();
    if (b.Width <= 0 || b.Height <= 0) return st::Rectangle();
    auto oPx = state.WorldSpace.GetScreenFromTile(obj->DrawPosition());
    return st::Rectangle(b.X + (int32_t)oPx.X, b.Y + (int32_t)oPx.Y, b.Width, b.Height);
}

void World2D::TakeStaticBounds(WorldComponent* comp, std::vector<st::Rectangle>& dirty, bool& fallback) {
    auto it = StaticBounds.find(comp);
    if (it == StaticBounds.end()) return; // never painted in the static layer
    (void)fallback;
    if (it->second.Width > 0) dirty.push_back(it->second);
    StaticBounds.erase(it);
}

void World2D::Invalidate() {
    StaticFloor.Valid = StaticWall.Valid = StaticObjects.Valid = false;
    CompositeValid = false;
    HaveOrigin = false;
}

st::Vector2 World2D::GetScrollIncrement(const st::Vector2& pxOffset, WorldState& state) {
    auto scrollSize = SCROLL_BUFFER / state.PreciseZoom();
    return st::Vector2((float)std::floor(pxOffset.X / scrollSize) * scrollSize, (float)std::floor(pxOffset.Y / scrollSize) * scrollSize);
}

// The view [px, px + view) must stay inside [origin, origin + view + SCROLL_BUFFER): origin moves in steps of
// SCROLL_BUFFER/4 and keeps a margin on both sides.
st::Vector2 World2D::GetBufferOrigin(const st::Vector2& px) const {
    int32_t step = std::max<int32_t>(1, SCROLL_BUFFER / 4);
    int32_t m = (SCROLL_BUFFER - step) / 2;
    auto f = [&](float v) { return (float)((int32_t)std::floor(v / step) * step - m); };
    return st::Vector2(f(px.X), f(px.Y));
}

void World2D::RenderLayer(int layer, WorldState& state, const st::Rectangle* clip) {
    auto _2d = state._2D;
    auto& buf = LayerBuffer(layer);
    auto& fb = buf.Buffer;
    int32_t bw = Composite.Width, bh = Composite.Height;
    if (clip == nullptr) {
        if (layer == OBJECTS) StaticBounds.clear();
        fb.Resize(bw, bh);
        if (fb.Room.size() != fb.Color.size()) fb.EnableRoom(true);
        fb.Clear(0);
        fb.ResetClip();
        buf.Valid = true;
    } else {
        fb.ClearRect(clip->X, clip->Y, clip->X + clip->Width, clip->Y + clip->Height, 0);
        fb.SetClip(clip->X, clip->Y, clip->X + clip->Width, clip->Y + clip->Height);
    }
    _2d->Target = &fb;
    _2d->SetScroll(Origin);
    switch (layer) {
        case FLOOR:
            /** Draw archetecture to a texture **/
            if (Blueprint->Terrain) {
                // FSO_CPP: the terrain mesh (3D in FreeSims) is rasterized straight into the floor layer
                Blueprint->Terrain->DepthMode = _2d->OutputDepth;
                Blueprint->Terrain->DrawInto(state, fb);
            }
            _2d->Begin(state.Camera());
            Blueprint->FloorComp->Draw(state);
            _2d->End();
            break;
        case WALL:
            _2d->Begin(state.Camera());
            Blueprint->WallComp->Draw(state);
            _2d->End();
            // FSO_CPP: World3D drew the roof every frame when State.DrawRoofs; here it lives in the cached wall layer.
            if (state.DrawRoofs) Blueprint->RoofComp.DrawInto(state, fb);
            break;
        case OBJECTS: {
            /** Draw static objects to a texture **/
            int32_t cx0 = std::max<int32_t>(0, fb.ClipX0), cy0 = std::max<int32_t>(0, fb.ClipY0);
            int32_t cx1 = std::min(fb.Width, fb.ClipX1), cy1 = std::min(fb.Height, fb.ClipY1);
            _2d->Begin(state.Camera());
            for (auto obj : Blueprint->Objects) {
                if (obj->Level > state.Level()) continue;
                if (obj->renderInfo.Layer == WorldObjectRenderLayer::STATIC) {
                    auto tilePosition = obj->DrawPosition();
                    auto oPx = state.WorldSpace.GetScreenFromTile(tilePosition);
                    if (clip != nullptr) {
                        obj->ValidateSprite(state);
                        auto b = obj->Bounding();
                        int32_t x = b.X + (int32_t)oPx.X - (int32_t)Origin.X, y = b.Y + (int32_t)oPx.Y - (int32_t)Origin.Y;
                        if (x >= cx1 || y >= cy1 || x + b.Width <= cx0 || y + b.Height <= cy0) continue;
                    }
                    _2d->OffsetPixel(oPx);
                    _2d->OffsetTile(tilePosition);
                    _2d->SetObjID(obj->ObjectID);
                    obj->Draw(state);
                    StaticBounds[obj] = ObjectScreenBounds(obj, state);
                }
            }
            _2d->End();
            break;
        }
    }
    fb.ResetClip();
    _2d->Target = nullptr;
}

void World2D::ShiftBuffer(Framebuffer& fb, int32_t sx, int32_t sy) {
    // content moves by (sx, sy): new[x + sx, y + sy] = old[x, y]
    int32_t W = fb.Width, H = fb.Height;
    if (W <= 0 || H <= 0) return;
    int32_t n = W - std::abs(sx);
    if (n <= 0 || std::abs(sy) >= H) return;
    int32_t dstX = std::max<int32_t>(0, sx), srcX = std::max<int32_t>(0, -sx);
    auto moveRow = [&](int32_t dy, int32_t syRow) {
        std::memmove(&fb.Color[(size_t)dy * W + dstX], &fb.Color[(size_t)syRow * W + srcX], (size_t)n * sizeof(uint32_t));
        std::memmove(&fb.Depth[(size_t)dy * W + dstX], &fb.Depth[(size_t)syRow * W + srcX], (size_t)n * sizeof(float));
        if (!fb.Room.empty()) std::memmove(&fb.Room[(size_t)dy * W + dstX], &fb.Room[(size_t)syRow * W + srcX], (size_t)n * sizeof(uint16_t));
    };
    if (sy > 0) { for (int32_t y = H - 1; y >= sy; y--) moveRow(y, y - sy); }
    else { for (int32_t y = 0; y < H + sy; y++) moveRow(y, y - sy); }
}

// drawSimpleRestoreDepth: copies a cached layer (colour + absolute depth) into dst with the depth test.
void World2D::MergeLayer(const Framebuffer& src, Framebuffer& dst, const st::Rectangle& rect) const {
    const uint16_t* rooms = src.Room.empty() ? nullptr : src.Room.data();
    int32_t x0 = std::max<int32_t>(0, rect.X), y0 = std::max<int32_t>(0, rect.Y);
    int32_t x1 = std::min(std::min(src.Width, dst.Width), rect.X + rect.Width), y1 = std::min(std::min(src.Height, dst.Height), rect.Y + rect.Height);
    for (int32_t y = y0; y < y1; y++) {
        const uint32_t* sc = &src.Color[(size_t)y * src.Width];
        const float* sd = &src.Depth[(size_t)y * src.Width];
        uint32_t* tc = &dst.Color[(size_t)y * dst.Width];
        float* td = &dst.Depth[(size_t)y * dst.Width];
        for (int32_t x = x0; x < x1; x++) {
            uint32_t c = sc[x];
            uint32_t a = c >> 24;
            if (a < 3) continue; // color.a < 0.01: discard
            float d = sd[x];
            if (d > td[x]) continue;
            td[x] = d;
            if (rooms) {
                // deferred ambient light (psZDepthSprite: colour * tex2D(ambientSampler, room))
                uint16_t room = rooms[(size_t)y * src.Width + x];
                if (room != 65535) {
                    const st::Color& l = (room == Framebuffer::OUTSIDE_LIGHT_ROOM) ? OutsideLight : AmbientLight[room];
                    uint32_t lr = l.R + (l.R >> 7), lg = l.G + (l.G >> 7), lb = l.B + (l.B >> 7), la = l.A + (l.A >> 7);
                    a = (a * la) >> 8;
                    c = (((c & 0xFF) * lr) >> 8) | (((((c >> 8) & 0xFF) * lg) >> 8) << 8) | (((((c >> 16) & 0xFF) * lb) >> 8) << 16) | (a << 24);
                }
            }
            if (a == 255) { tc[x] = c; continue; }
            uint32_t dc = tc[x], ia = 255 - a;
            uint32_t r = ((c & 0xFF) * a + (dc & 0xFF) * ia) / 255;
            uint32_t g = (((c >> 8) & 0xFF) * a + ((dc >> 8) & 0xFF) * ia) / 255;
            uint32_t b = (((c >> 16) & 0xFF) * a + ((dc >> 16) & 0xFF) * ia) / 255;
            uint32_t na = std::min<uint32_t>(255, a + ((dc >> 24) * ia) / 255);
            tc[x] = Framebuffer::Pack((uint8_t)r, (uint8_t)g, (uint8_t)b, (uint8_t)na);
        }
    }
}

void World2D::BuildComposite(const st::Rectangle& rect) {
    Composite.ClearRect(rect.X, rect.Y, rect.X + rect.Width, rect.Y + rect.Height, ClearColor);
    MergeLayer(StaticFloor.Buffer, Composite, rect);
    MergeLayer(StaticWall.Buffer, Composite, rect);
    MergeLayer(StaticObjects.Buffer, Composite, rect);
}

void World2D::Draw(WorldState& state, Framebuffer& target) {
    if (Blueprint == nullptr || !CompositeValid) { target.Clear(ClearColor); return; }
    auto _2d = state._2D;
    auto pxOffset = state.WorldSpace.GetScreenOffset();
    pxOffset = st::Vector2(-pxOffset.X, -pxOffset.Y);

    /**
     * Draw static layers: one row copy of the composite (the view is always inside it).
     */
    int32_t ox = (int32_t)(pxOffset.X - Origin.X), oy = (int32_t)(pxOffset.Y - Origin.Y);
    for (int32_t y = 0; y < target.Height; y++) {
        int32_t sy = y + oy;
        uint32_t* tc = &target.Color[(size_t)y * target.Width];
        float* td = &target.Depth[(size_t)y * target.Width];
        if (sy < 0 || sy >= Composite.Height) {
            std::fill(tc, tc + target.Width, ClearColor);
            std::fill(td, td + target.Width, std::numeric_limits<float>::infinity());
            continue;
        }
        int32_t x0 = std::max<int32_t>(0, -ox), x1 = std::min(target.Width, Composite.Width - ox);
        if (x0 > 0) { std::fill(tc, tc + x0, ClearColor); std::fill(td, td + x0, std::numeric_limits<float>::infinity()); }
        if (x1 < target.Width) { std::fill(tc + std::max<int32_t>(0, x1), tc + target.Width, ClearColor); std::fill(td + std::max<int32_t>(0, x1), td + target.Width, std::numeric_limits<float>::infinity()); }
        if (x1 > x0) {
            std::memcpy(tc + x0, &Composite.Color[(size_t)sy * Composite.Width + x0 + ox], (size_t)(x1 - x0) * sizeof(uint32_t));
            std::memcpy(td + x0, &Composite.Depth[(size_t)sy * Composite.Width + x0 + ox], (size_t)(x1 - x0) * sizeof(float));
        }
    }

    /**
     * Draw dynamic objects. If an object has been static for X frames move it back into the static layer
     */
    _2d->Target = &target;
    _2d->SetScroll(pxOffset);
    _2d->Begin(state.Camera());

    st::Rectangle worldBounds((int32_t)pxOffset.X, (int32_t)pxOffset.Y, (int32_t)state.WorldSpace.WorldPxWidth, (int32_t)state.WorldSpace.WorldPxHeight);

    for (auto obj : Blueprint->Objects) {
        if (obj->Level > state.Level()) continue;
        if (obj->renderInfo.Layer == WorldObjectRenderLayer::DYNAMIC) {
            auto tilePosition = obj->DrawPosition();
            auto oPx = state.WorldSpace.GetScreenFromTile(tilePosition);
            obj->ValidateSprite(state);
            auto b = obj->Bounding();
            st::Rectangle offBound(b.X + (int32_t)oPx.X, b.Y + (int32_t)oPx.Y, b.Width, b.Height);
            if (!(offBound.X < worldBounds.Right() && worldBounds.X < offBound.Right() &&
                  offBound.Y < worldBounds.Bottom() && worldBounds.Y < offBound.Bottom())) continue;
            _2d->OffsetPixel(oPx);
            _2d->OffsetTile(tilePosition);
            _2d->SetObjID(obj->ObjectID);
            obj->Draw(state);
        }
    }
    _2d->End();
    _2d->Target = nullptr;
}

}}
