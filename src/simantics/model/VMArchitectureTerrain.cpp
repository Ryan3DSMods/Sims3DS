/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
// FSO.SimAntics.Model.VMArchitectureTerrain (FreeSO) — see the header.
#include "simantics/model/VMArchitectureTerrain.h"

namespace FSO { namespace SimAntics { namespace Model {

VMArchitectureTerrain::VMArchitectureTerrain(int32_t width, int32_t height) : Width(width), Height(height) {
    size_t n = (size_t)(width * height);
    Heights.assign(n, 0);
    GrassState.assign(n, 0);
    Centers.assign(n, 0);
    Sloped.assign(n, false);
    VisHeights.assign(n, 0);
    VisGrass.assign(n, 0);
}

void VMArchitectureTerrain::EnterVis() {
    OldHeights = Heights;
    OldGrass = GrassState;
    VisHeights = Heights;
    VisGrass = GrassState;
}

void VMArchitectureTerrain::ExitVis() {
    VisHeights = Heights;   // the commands ran on the live arrays: they are the preview
    VisGrass = GrassState;
    Heights.swap(OldHeights);
    GrassState.swap(OldGrass);
    OldHeights.clear(); OldHeights.shrink_to_fit();
    OldGrass.clear(); OldGrass.shrink_to_fit();
    RegenerateCenters();
}

void VMArchitectureTerrain::RegenerateCenters() { //TODO: partial update
    int i = 0;
    for (int y = 0; y < Height; y++) {
        for (int x = 0; x < Width; x++) {
            auto h1 = Heights[(size_t)i];
            auto h2 = Heights[(size_t)(y * Width + ((x + 1) % Width))];
            auto h3 = Heights[(size_t)(((y + 1) % Height) * Width + ((x + 1) % Width))];
            auto h4 = Heights[(size_t)(((y + 1) % Height) * Width + x)];

            Centers[(size_t)i] = (int16_t)((Heights[(size_t)i] + h2 + h3 + h4) / 4);
            Sloped[(size_t)i] = (h1 != h2) || (h1 != h3) || (h1 != h4);
            i++;
        }
    }
}

}}}
