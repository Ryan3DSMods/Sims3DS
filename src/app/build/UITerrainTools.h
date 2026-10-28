/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
// sims app — FreeSO FSO.UI/Panels/LotControls/UITerrainRaiser.cs, UITerrainFlatten.cs, UIGrassPaint.cs (FreeSims
// has no terrain tools). Terrain heights live on the tile corners (vertices); the tools send FreeSO's TERRAIN_RAISE /
// TERRAIN_FLATTEN / GRASS_DOT architecture commands, previewed through VMArchitecture.Commands like the other tools.
// TS1(sims): The Sims has separate Raise / Lower / Level tools (Build.iff STR# 128: "Click or click and drag to
// raise the terrain level. Shift-click to lower terrain again."): click raises the vertex (or the dragged rectangle of
// vertices) by one step, holding the stylus keeps raising (TS1 behaviour); "Shift" (the toolbar toggle) inverts.
// FreeSO's raiser instead drags the height with the mouse. The TSO terrain cursor object (0x2F39B7A6) does not exist
// in TS1: the vertex markers are drawn by UIBuildMode on the top screen (Marker*), the cursor is SimsRaise/Lower/Level.cur.
#pragma once
#include <cstdint>
#include <map>
#include <vector>
#include "app/build/UICustomLotControl.h"
#include "simantics/model/VMArchitectureCommand.h"

namespace simapp {

class LotSession;

class UITerrainTool : public UICustomLotControl {
public:
    enum class Mode { Raise = 0, Lower = 1, Level = 2, Grass = 3 };
    UITerrainTool(LotSession* session, Mode mode);
    void MouseDown(LotControlState& state) override;
    void MouseUp(LotControlState& state) override;
    void Update(LotControlState& state, bool scrolled) override;
    void Release() override;

    Mode ToolMode;
    int LastCost = 0;
    bool LastPlaced = false;
    int LastFailReason = 0;
    // vertex rectangle for the top-screen markers (tile corner coordinates)
    int MarkerX0 = -1, MarkerY0 = -1, MarkerX1 = -1, MarkerY1 = -1;
    bool MarkerError = false;

    /// TS1 raise / lower step in altitude units (x10 in the engine), and frames of holding per extra step.
    static const int StepUnits = 2;
    static const int HoldFrames = 10;

private:
    LotSession* Session;
    bool Drawing = false;
    int StartX = 0, StartY = 0;
    int16_t StartTerrainHeight = 0;
    int HeldFrames = 0;
    int _SoundSteps = 0;          // FIX(sims) SND: raise/lower steps already sounded this drag
    bool _WasDisallowed = false;  // FIX(sims) SND
    int LastCurX = -9999, LastCurY = -9999;
    std::vector<FSO::SimAntics::Model::VMArchitectureCommand> Commands;
    std::map<std::pair<int, int>, FSO::SimAntics::Model::VMArchitectureCommand> GrassCommands;
    void Cursor(const LotControlState& state, int& x, int& y, float& fx, float& fy);
};

}
