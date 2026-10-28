/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
// sims app — FreeSims UI/Panels/UIBuildMode.cs: the build mode panel. TS1's tool buttons (UIText STR# 139
// BldTips; art cpanel\Buttons\toolBtn*.bmp): Wall & Fence, Wallpaper, Floor, Door, Window, Stair, Fireplace, Plant
// and the Hand tool, each with its catalogue (wall styles + columns, wallpapers from walls.iff + Walls\*.wll,
// floors from floors.iff + Floors\*.flr, and the OBJD BuildModeType objects). Wall styles, wallpapers and floors
// hand the lot to a UICustomLotControl (UIWallPlacer / UIWallPainter / UIFloorPainter); objects use the object
// holder like buy mode. Costs come from the architecture simulation (VMArchitecture.LastTestCost).
// ADDED(sims): drawing happens in a touch view of the lot (or with the cursor + A); the keyboard modifiers
// are toolbar toggles (Erase = Ctrl, Rectangle / Fill = Shift). Terrain, water (pools) and roof tools are not
// ported. The game is paused in build mode (TS1), so the architecture preview is ticked here.
#pragma once
#include <map>
#include <memory>
#include <string>
#include <vector>
#include "app/ui/UIModePanel.h"
#include "app/buy/UICatalog.h"
#include "app/build/UICustomLotControl.h"

namespace simapp {

class UIBuildMode : public UIModePanel {
public:
    // TS1(sims): Terrain / Water (pool) / Roof appended so the "toolN" ids stay stable; drawn in TS1's order
    enum class Tool { None = -1, Wall = 0, Wallpaper, Floor, Door, Window, Stairs, Fireplace, Plant, Hand, Terrain, Pool, Roof, Count };
    explicit UIBuildMode(LotScreen* owner);
    ~UIBuildMode();
    void Activate() override;
    void Deactivate() override;
    void Draw(Canvas& c, const Rect& area) override;
    bool Tap(const std::string& id) override;
    void PointerUpdate() override;
    void DrawTop(Canvas& top) override;
    bool Buttons(const AppInput& in) override;
    bool ViewDragScrolls() const override;
    std::string CursorArt() const override;

    Tool CurrentTool = Tool::None;
    int Page = 0;
    int Selection = -1;
    std::vector<const UICatalogElement*> Current;
    std::unique_ptr<UICustomLotControl> Control;   // C# UILotControl.CustomControl
    LotControlState ControlState;
    bool Erase = false, Shift = false;

    bool Select(int index);
    void SetTool(Tool t);
    // TS1(sims): the sub-tools of the Terrain / Water / Roof tools (cpanel\Build art, Build.iff STR# 128 / 133)
    struct SubTool { std::string Art, Popup, Cursor, Name, Desc; int Price = 0; int Id = 0; };
    std::vector<SubTool> SubTools;
    int SubSelection = -1;
    bool SelectSub(int index);
    /// Roof tool: pitch (TS1 HOUS: 0.4 shallow, 0.6 medium, 1.0 steep, 0 flat) and style (GameData/Roofs/*.bmp)
    void SetRoof(float pitch, int style);

private:
    int _Pages = 1;
    bool _WasHeld = false;
    int _LastTileX = -9999, _LastTileY = -9999;
    bool _TapPlaceArmed = false;
    const UICatalogElement* _ActiveElement = nullptr;
    void ReleaseControl();
    void Refresh();
    void DrawSubTools(Canvas& c, const Rect& grid);
    void DrawRoofPanel(Canvas& c, const Rect& grid);
    void DrawTerrainMarkers(Canvas& top);
    const Canvas* RoofIcon(int style);
    std::map<int, std::unique_ptr<Canvas>> _RoofIcons;
    int _RoofPage = 0;
};

}
