/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
// FSO.LotView.Model.Blueprint (World/model/Blueprint.cs)
#include "world/model/Blueprint.h"
#include <cmath>
#include <algorithm>

namespace FSO { namespace LotView { namespace Model {

Blueprint::Blueprint(int32_t width, int32_t height) : Width(width), Height(height) {
    auto numTiles = width * height;
    this->WallComp.reset(new Components::WallComponent());
    WallComp->blueprint = this;
    this->FloorComp.reset(new Components::FloorComponent());
    FloorComp->blueprint = this;
    RoofComp.blueprint = this;

    RoomColors.assign(65536, st::Color()); // C# new Color[65536]: transparent black
    RoomMap.resize((size_t)Stories, nullptr);
    this->Cutaway.assign((size_t)numTiles, false);
}

Blueprint::~Blueprint() {}

void Blueprint::AddAvatar(Components::AvatarComponent* avatar) {
    avatar->AltitudeSource = this; // TS1(sims): terrain altitude in DrawPosition
    Avatars.push_back(avatar);
}

float Blueprint::GetAltitude(int x, int y) const {
    if (AltitudeCenters == nullptr || AltitudeCenters->empty() || x <= 0 || y <= 0) return 0.0f;
    size_t i = (size_t)((y % Height) * Width + (x % Width));
    if (i >= AltitudeCenters->size()) return 0.0f;
    return ((*AltitudeCenters)[i] - BaseAlt) * TerrainFactor;
}

float Blueprint::GetAltPoint(int x, int y) const {
    if (Altitude == nullptr || Altitude->empty() || x <= 0 || y <= 0) return 0.0f;
    size_t i = (size_t)((y % Height) * Width + (x % Width));
    return (i < Altitude->size()) ? (float)(*Altitude)[i] : 0.0f;
}

float Blueprint::InterpAltitude(const st::Vector3& Position) const {
    if (Altitude == nullptr || Altitude->empty()) return 0.0f;
    auto baseX = (int)std::max(1.0f, std::min((float)(Width - 1), Position.X));
    auto baseY = (int)std::max(1.0f, std::min((float)(Height - 1), Position.Y));
    if (baseX < 0 || baseY < 0) return 0;
    auto nextX = (int)std::max(1.0f, std::min((float)(Width - 1), std::ceil(Position.X)));
    auto nextY = (int)std::max(1.0f, std::min((float)(Height - 1), std::ceil(Position.Y)));
    float xLerp = std::fmod(Position.X, 1.0f);
    float yLerp = std::fmod(Position.Y, 1.0f);

    auto by = (baseY % Height) * Width;
    auto bx = (baseX % Width);
    auto ny = (nextY % Height) * Width;
    auto nx = (nextX % Width);
    auto& A = *Altitude;
    float h00 = A[(size_t)(by + bx)];
    float h01 = A[(size_t)(ny + bx)];
    float h10 = A[(size_t)(by + nx)];
    float h11 = A[(size_t)(ny + nx)];

    float xl1 = xLerp * h10 + (1 - xLerp) * h00;
    float xl2 = xLerp * h11 + (1 - xLerp) * h01;

    return (yLerp * xl2 + (1 - yLerp) * xl1 - BaseAlt) * TerrainFactor;
}

void Blueprint::GenerateRoomLights() {
    float len = (float)std::sqrt((double)(OutsideColor.R * OutsideColor.R + OutsideColor.G * OutsideColor.G + OutsideColor.B * OutsideColor.B));
    auto minOut = OutsideColor * (float)(len > 0 ? 150 / len : 0);

    for (size_t i = 0; i < Light.size() && i < RoomColors.size(); i++) {
        auto outside = OutsideColor * (Light[i].OutsideLight / 100.0f);
        auto ambient = st::Color::White() * (Light[i].AmbientLight / 100.0f);

        outside.R = std::max(minOut.R, outside.R);
        outside.G = std::max(minOut.G, outside.G);
        outside.B = std::max(minOut.B, outside.B);

        RoomColors[i] = st::Color(
            std::min(255, outside.R + ambient.R),
            std::min(255, outside.G + ambient.G),
            std::min(255, outside.B + ambient.B),
            255);
    }
    RoomColors[65535] = st::Color::White();
}

void Blueprint::ChangeObjectLocation(Components::ObjectComponent* component, const LotTilePos& pos) {
    int16_t tileX = (pos.x < 0) ? (int16_t)0 : pos.TileX();
    int16_t tileY = (pos.y < 0) ? (int16_t)0 : pos.TileY();
    int8_t level = pos.Level;

    Damage.push_back(BlueprintDamage(BlueprintDamageType::OBJECT_MOVE, tileX, tileY, level, component));

    component->blueprint = this;
    component->AltitudeSource = this;
    component->TileX = tileX;
    component->TileY = tileY;
    component->Level = level;
}

void Blueprint::AddObject(Components::ObjectComponent* component) {
    // FSO_CPP: C# sets component.blueprint in ChangeObjectLocation only; objects that never move
    // (out of world) still need it for damage, harmless to set here.
    component->blueprint = this;
    component->AltitudeSource = this;
    Objects.push_back(component);
}

void Blueprint::RemoveObject(Components::ObjectComponent* component) {
    Damage.push_back(BlueprintDamage(BlueprintDamageType::OBJECT_MOVE, component->TileX, component->TileY, component->Level, component));
    for (size_t i = 0; i < Objects.size(); i++) if (Objects[i] == component) { Objects.erase(Objects.begin() + i); return; }
}

}}}
