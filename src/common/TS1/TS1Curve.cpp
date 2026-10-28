/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
// TS1(sims): translated from FreeSO tso.common/TS1/TS1Curve.cs
#include "common/TS1/TS1Curve.h"
#include <algorithm>
#include <cstdlib>

namespace FSO { namespace Common { namespace TS1 {

TS1Curve::TS1Curve(const std::string& inputIn) {
    // C#: input.Replace(");(", ") (") then Split(' '); each "(x;y)" -> Point. Empty entries would throw in C#;
    // they are skipped here.
    std::string input = inputIn;
    size_t pos;
    while ((pos = input.find(");(")) != std::string::npos) input.replace(pos, 3, ") (");
    size_t i = 0;
    while (i < input.size()) {
        while (i < input.size() && input[i] == ' ') i++;
        size_t end = input.find(' ', i);
        if (end == std::string::npos) end = input.size();
        std::string pointStr = input.substr(i, end - i);
        i = end;
        if (pointStr.size() < 2) continue;
        auto pointClean = pointStr.substr(1, pointStr.size() - 2);
        auto split = pointClean.find(';');
        if (split == std::string::npos) continue;
        Point pt;
        pt.X = std::atoi(pointClean.substr(0, split).c_str());
        pt.Y = std::atoi(pointClean.substr(split + 1).c_str());
        Points.push_back(pt);
    }
    std::stable_sort(Points.begin(), Points.end(), [](const Point& a, const Point& b) { return a.X < b.X; });
}

float TS1Curve::GetPoint(float input) const {
    if (Points.empty()) return 0;
    if (input < Points[0].X) return (float)Points[0].Y;
    // find the first point we're ahead of
    // we want to interpolate between that point and the one after it
    int i = 0;
    for (i = (int)Points.size() - 1; i >= 0; i--) {
        auto& point = Points[i];
        if (input >= point.X) break;
    }
    if (i == (int)Points.size() - 1) return (float)Points[i].Y;

    auto& start = Points[i];
    auto& end = Points[i + 1];
    auto iF = (input - start.X) / (float)(end.X - start.X);
    return (1 - iF) * start.Y + iF * end.Y;
}

int32_t TS1Curve::GetPointFixed(int32_t input) const {
    if (Points.empty()) return 0;
    if (input < Points[0].X) return Points[0].Y;
    // NOTE: translated as written in C# (the loop direction differs from GetPoint; unused by the VM).
    int i = 0;
    for (i = 0; i < (int)Points.size(); i++) {
        auto& point = Points[i];
        if (input >= point.X) break;
    }
    if (i >= (int)Points.size() - 1) return Points[std::min(i, (int)Points.size() - 1)].Y;

    auto& start = Points[i];
    auto& end = Points[i + 1];
    auto iF = ((input - start.X) * 65536) / (end.X - start.X);
    return (65536 - iF) * start.Y + iF * end.Y;
}

void TS1Curve::BuildCache(int32_t min, int32_t max) {
    Cache.assign((size_t)((max - min) + 1), 0.0f);
    CacheFixed.assign((size_t)((max - min) + 1), 0);
    CacheStart = min;
    for (int32_t i = min; i <= max; i++) {
        Cache[i - min] = GetPoint((float)i);
        CacheFixed[i - min] = GetPointFixed(i);
    }
}

float TS1Curve::GetCachedPoint(int32_t point) const {
    // FIX(sims): C# indexed Cache[point] after clamping, without subtracting CacheStart.
    point = std::max(CacheStart, std::min(CacheStart + (int32_t)Cache.size() - 1, point));
    return Cache[point - CacheStart];
}

int32_t TS1Curve::GetCachedPointFixed(int32_t point) const {
    point = std::max(CacheStart, std::min(CacheStart + (int32_t)CacheFixed.size() - 1, point));
    return CacheFixed[point - CacheStart];
}

}}}
