/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
// FSO.Common.Utils.TextureGenerator — world z-buffers / air tiles, see TextureGenerator.h
#include "world/utils/TextureGenerator.h"
#include <cmath>
#include <algorithm>

namespace FSO { namespace Common { namespace Utils {

using FSO::LotView::Utils::Texture2D;

const float TextureGenerator::FLAT_Z_INC = 1.525f;
#define FLAT_Z_INC_ 1.525f

const float TextureGenerator::WallZBufferConfig[31][5] = {
    // format: width, height, startIntensity, Xdiff, Ydiff

    {64, 271, 74, 1, 0.5f}, //near top left
    {64, 271, 135, -1, 0.5f}, //near top right
    {128, 240, 89.5f, 0, 0.5f}, //near horiz diag
    {16, 232, 45, 0, 0.5f}, //near vert diag

    {32, 135, 74, 2, 1.0f}, //med top left
    {32, 135, 135, -2, 1.0f}, //med top right
    {64, 120, 89.5f, 0, 1.0f}, //med horiz diag
    {8, 116, 45, 0, 1.0f}, //med vert diag

    {16, 67, 74, 4, 2.0f}, //far top left
    {16, 67, 135, -4, 2.0f}, //far top right
    {32, 60, 89.5f, 0, 2.0f}, //far horiz diag
    {4, 58, 45, 0, 2.0f}, //far vert diag

    //12
    {128, 64, 255, 0, -FLAT_Z_INC_}, //near floor
    {64, 32, 255, 0, -FLAT_Z_INC_ * 2}, //med floor
    {32, 16, 255, 0, -FLAT_Z_INC_ * 4}, //far floor

    //vert flips of the above
    //15
    {128, 64, 153, 0, FLAT_Z_INC_},
    {64, 32, 153, 0, FLAT_Z_INC_ * 2},
    {32, 16, 153, 0, FLAT_Z_INC_ * 4},

    //18
    {128, 64, 257, 0, -FLAT_Z_INC_}, //near junction walls up
    {64, 32, 257, 0, -FLAT_Z_INC_ * 2}, //med junction walls up
    {32, 16, 257, 0, -FLAT_Z_INC_ * 4}, //far junction walls up

    //versions for corners (man this is getting complicated)
    //21
    //top corner
    {43, 22, 254, 0, -FLAT_Z_INC_}, //near
    {21, 12, 254, 0, -FLAT_Z_INC_ * 2}, //med
    {13, 7, 254, 0, -FLAT_Z_INC_ * 4}, //far

    //24
    //side corner
    {35, 21, 254 - (FLAT_Z_INC_ * 22), 0, -FLAT_Z_INC_}, //near
    {16, 13, 254 - (FLAT_Z_INC_ * 22), 0, -FLAT_Z_INC_ * 2}, //med
    {11, 8, 254 - (FLAT_Z_INC_ * 22), 0, -FLAT_Z_INC_ * 4}, //far

    //27
    {41, 23, 254 - (FLAT_Z_INC_ * (64 - 23)), 0, -FLAT_Z_INC_}, //near
    {18, 13, 254 - (FLAT_Z_INC_ * (64 - 23)), 0, -FLAT_Z_INC_ * 2}, //med
    {9, 8, 254 - (FLAT_Z_INC_ * (64 - 23)), 0, -FLAT_Z_INC_ * 4}, //far

    //30
    {1, 1, 49, 0, 0} //balloon
};

std::vector<std::vector<uint8_t>> TextureGenerator::WallZBufferData;
std::vector<Texture2D> TextureGenerator::WallZBuffer;
std::vector<std::vector<st::Color>> TextureGenerator::AirTileData;
std::vector<Texture2D> TextureGenerator::AirTiles;

std::vector<Texture2D>& TextureGenerator::GetWallZBuffer() {
    if (WallZBuffer.empty()) {
        const int count = 31;
        WallZBufferData.resize(count);
        WallZBuffer.resize(count);
        for (int i = 0; i < count; i++) {
            auto& config = WallZBufferConfig[i];
            int width = (int)config[0];
            int height = (int)config[1];

            auto& data = WallZBufferData[i];
            data.resize((size_t)width * height);
            int offset = 0;

            float yInt = config[2];
            for (int y = 0; y < height; y++) {
                float xInt = yInt;
                for (int x = 0; x < width; x++) {
                    // C#: (byte)Math.Round(Math.Min(255, xInt)) — negative values wrap like the C# byte cast
                    data[(size_t)offset++] = (uint8_t)(int)std::nearbyint(std::min(255.0f, xInt));
                    xInt += config[3];
                }
                yInt += config[4];
            }
            WallZBuffer[i].Width = width;
            WallZBuffer[i].Height = height;
            WallZBuffer[i].Alpha8 = data.data();
        }
    }
    return WallZBuffer;
}

std::vector<Texture2D>& TextureGenerator::GetAirTiles() {
    if (AirTiles.empty()) {
        AirTileData.resize(3);
        AirTiles.resize(3);
        int dims[3][2] = { {127, 64}, {63, 32}, {31, 16} };
        for (int i = 0; i < 3; i++) {
            GenerateAirTile(dims[i][0], dims[i][1], AirTileData[i]);
            AirTiles[i].Width = dims[i][0];
            AirTiles[i].Height = dims[i][1];
            AirTiles[i].Data = AirTileData[i].data();
        }
    }
    return AirTiles;
}

void TextureGenerator::GenerateAirTile(int width, int height, std::vector<st::Color>& data) {
    data.assign((size_t)width * height, st::Color());
    int center = width / 2;
    int middleOff = 0;
    for (int i = 0; i < height; i++) {
        int index = i * width + (center - middleOff);
        for (int j = 0; j < ((middleOff == 0) ? 1 : 2); j++)
            data[(size_t)index++] = (i + j > height / 2) ? st::Color::Black() : st::Color::White();
        for (int j = 0; j < (middleOff * 2) - 3; j++)
            if (i % 2 == 0 && (i + (center - middleOff) + j) % 4 == 0) data[(size_t)index++] = st::Color::Black();
            else index++;
        if (middleOff != 0) {
            for (int j = 0; j < 2; j++)
                data[(size_t)index++] = (i + (1 - j) > height / 2) ? st::Color::Black() : st::Color::White();
        }
        middleOff += (i == height / 2 - 1) ? 1 : ((i < height / 2) ? 2 : -2);
    }
}

}}}
