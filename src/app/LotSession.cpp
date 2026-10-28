/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
// sims app — see LotSession.h
#include "files/SafeFile.h" // FIX(sims) SAVE
#include "files/LoadProgress.h"
#include <algorithm>
#include "app/LotSession.h"
#include "app/WorldPicking.h"
#include "app/live/UIHeadlineRenderer.h"
#include "simantics/engine/VMTS1Neighbourhood.h"
#include "content/ContentManager.h"
#include "content/TS1NeighbourProvider.h"
#include "simantics/VM.h"
#include "simantics/VMContext.h"
#include "simantics/VMArchitecture.h"
#include "simantics/model/VMRuntimeHeadline.h"
#include "simantics/model/VMDialogInfo.h"
#include "simantics/utils/VMWorldActivator.h"
#include "simantics/utils/VMArchitectureTools.h"
#include "simantics/utils/VMTS1FamilyLoader.h"
#include "simantics/engine/VMTS1Travel.h"
#include "simantics/entities/VMEntity.h"
#include "simantics/entities/VMAvatar.h"
#include "simantics/entities/VMGameObject.h"
#include "simantics/entities/VMMultitileGroup.h"
#include "simantics/engine/VMThread.h"
#include "simantics/engine/VMQueuedAction.h"
#include "simantics/engine/VMDialogOperand.h"
#include "files/formats/iff/IffFile.h"
#include "world/World.h"
#include "world/model/Blueprint.h"
#include "world/components/AvatarComponent.h"
#include <unordered_set>
#include "simantics/VMClock.h"
#include "app/front/FrontStrings.h"
#include "simantics/model/VMStackObjectVariable.h"
#include "files/formats/iff/chunks/OBJD.h"
#include <cmath>
#include <cstdio>
#include <cstdlib>

namespace simapp {

using namespace FSO::SimAntics;
using FSO::LotView::Model::LotTilePos;
using FSO::LotView::Model::Direction;
using FSO::LotView::WorldZoom;
using FSO::LotView::WorldRotation;

static const uint32_t GOTO_GUID = 0x000007C4; // UILotControl.GOTO_GUID

LotSession::LotSession() {}

LotSession::~LotSession() {
    // the VM's entities reference the context, world components and the house's chunks
    _VM.reset();
    _Ctx.reset();
    _Headlines.reset();
    _World.reset();
    _House.reset();
}

bool LotSession::Load(int houseNumber, int viewW, int viewH, int scrollBuffer, std::string& error, bool travel) {
    auto content = FSO::Content::Content::Get();
    auto hood = content ? content->Neighborhood.get() : nullptr;
    HouseNumber = houseNumber;
    std::string path;
    if (hood != nullptr) path = hood->GetHousePath(houseNumber);
    if (path.empty()) {
        char buf[64]; snprintf(buf, sizeof buf, "/UserData/Houses/House%02d.iff", houseNumber);
        path = content->BasePath + buf;
    }
    const std::string lotTag = "lot " + std::to_string(houseNumber) + ": "; // TS1(sims): step log (files/LoadProgress.h)
    FSO::Files::LoadProgress::Step(lotTag + "house file");
    // FIX(sims) SAVE: an interrupted house save left House<N>.iff missing (the template was loaded instead) or cut:
    // put the newest complete copy back (.new / .tmp / .bak; NhoodModel::Switch does this for the whole folder once).
    if (hood != nullptr) {
        char rel[48]; snprintf(rel, sizeof rel, "/Houses/House%02d.iff", houseNumber);
        std::string mine = hood->UserPath + rel;
        if (FSO::Files::SafeFile::HasLeftover(mine) &&
            FSO::Files::SafeFile::Recover(mine, FSO::Files::SafeFile::IffValidator()) == FSO::Files::SafeFile::Result::Restored)
            path = hood->GetHousePath(houseNumber);
    }
    try { _House.reset(new FSO::Files::Formats::IFF::IffFile(path)); }
    catch (std::exception& e) { error = "Cannot open " + path + ": " + e.what(); return false; }

    _World.reset(new FSO::LotView::World(viewW, viewH));
    Frame.Resize(viewW, viewH);
    _World->SetScrollBuffer(scrollBuffer);
    VM::UseWorld(true);
    _Ctx.reset(new VMContext(_World.get()));
    _Headlines.reset(new UIHeadlineRendererProvider()); // FreeSims UIHeadlineRendererProvider (balloons)
    _VM.reset(new FSO::SimAntics::VM(_Ctx.get(), _Headlines.get()));
    _VM->Init();
    _VM->OnDialog = [this](Model::VMDialogInfo& info) {
        if (info.Title == "SimAntics Exception!") { // object resets: logged, not shown (FreeSims shows them in debug)
            static int shown = 0; // TS1(sims) QA: SIM3DS_RESETLOG=N prints the first N reset reports (stack) to stdout
            static const int maxShown = getenv("SIM3DS_RESETLOG") ? atoi(getenv("SIM3DS_RESETLOG")) : 0;
            if (shown < maxShown) { shown++; printf("[reset] %s\n", info.Message.c_str()); }
            return;
        }
        PendingDialog d;
        d.Title = info.Title; d.Message = info.Message;
        d.Yes = info.Yes; d.HasYes = info.HasYes;
        d.No = info.No; d.HasNo = info.HasNo;
        d.Cancel = info.Cancel; d.HasCancel = info.HasCancel;
        d.Block = info.Block;
        d.CallerID = info.Caller ? info.Caller->ObjectID : 0;
        d.Type = (uint8_t)info.Operand.Type;
        // FIX(sims): the buttons come from the dialog type, a string id of 0 means the default label (FreeSO
        // UILotControl.ShowDialog / DialogDescriptors). Before, a Yes/No dialog without button strings (the Help
        // System's "Would you like the help system activated?") showed only "OK", which always answered Yes.
        {
            auto& T = simapp::front::FrontStrings::Get();
            auto t = info.Operand.Type;
            if (t == FSO::SimAntics::Primitives::VMDialogType::YesNo || t == FSO::SimAntics::Primitives::VMDialogType::YesNoCancel) {
                if (!d.HasYes || d.Yes.empty()) { d.Yes = T.Get(152, 2, "Yes"); d.HasYes = true; }
                if (!d.HasNo || d.No.empty()) { d.No = T.Get(152, 3, "No"); d.HasNo = true; }
                if (t == FSO::SimAntics::Primitives::VMDialogType::YesNoCancel && (!d.HasCancel || d.Cancel.empty())) { d.Cancel = T.Get(152, 1, "Cancel"); d.HasCancel = true; }
            }
        }
        d.IconID = info.Icon ? info.Icon->ObjectID : 0;
        if (d.Type == 6 && info.Caller != nullptr) {
            // TS1(sims): the dressers / racks set the outfit type in temp 0 and read the answer (an index into
            // VMTS1Outfits::ValidOutfits) from temp 1 for Generic Sims Call 24/29 (Behavior.iff STR#220)
            d.AvatarID = info.Caller->ObjectID;
            if (info.Caller->Thread != nullptr) d.OutfitType = info.Caller->Thread->TempRegisters[0];
            if (dynamic_cast<VMAvatar*>(info.Caller) == nullptr && info.Caller->Thread && info.Caller->Thread->Stack.size() > 0) {
                auto so = info.Caller->Thread->Stack.back()->StackObject;
                if (so) d.AvatarID = so->ObjectID;
            }
        }
        if (Dialogs.size() < 8) Dialogs.push_back(d);
    };
    _VM->OnSimanticsException = [this](VMEntity*, VMEntity*, const std::string&) { Resets++; };

    FSO::Files::LoadProgress::Step(lotTag + "objects + architecture");
    try {
        VMWorldActivator act(_VM.get(), _World.get());
        Blueprint = act.LoadFromIff(_House.get());
    } catch (std::exception& e) { error = std::string("Lot load failed: ") + e.what(); return false; }
    if (Blueprint == nullptr) { error = "Lot load failed"; return false; }

    // TS1(sims): the travellers arriving on an expansion lot are spawned by its ped markers a few ticks later
    // and selected through VMTS1Travel::SelectSim (EventSelectedSimChanged): add them to the controllable Sims.
    _VM->OnTS1Event = [this](int32_t evt, int32_t value) {
        if (evt != Engine::Utils::VMTS1Travel::EventSelectedSimChanged) return;
        auto av = dynamic_cast<VMAvatar*>(_VM->GetObjectById((int16_t)value));
        if (av == nullptr) return;
        int idx = IndexOfFamily(av);
        // FIX(sims): the camera follows the first traveller who arrives (it stayed on the lot's saved view, often
        // a corner without the Sim, e.g. Downtown house 23)
        if (idx < 0) { bool first = Family.empty(); Family.push_back(av); idx = (int)Family.size() - 1; if (first) CenterOn(av); }
        Selected = idx;
    };
    FSO::Files::LoadProgress::Step(lotTag + (travel ? "travellers" : "family"));
    if (travel) {
        std::vector<std::string> log;
        try { Family = Engine::Utils::VMTS1Travel::Arrive(_VM.get(), (int16_t)houseNumber, &log); }
        catch (std::exception& e) { printf("[app] travel arrival failed: %s\n", e.what()); }
        for (auto& l : log) printf("[travel] %s\n", l.c_str());
        auto fam = hood != nullptr ? hood->GameState.ActiveFamily : nullptr;
        if (fam != nullptr && hood != nullptr) FamilyName = hood->GetFamilyName((int16_t)fam->ChunkID);
    } else {
        VMTS1FamilyLoader loader(_VM.get());
        try { Family = loader.LoadFamily((int16_t)houseNumber); }
        catch (std::exception& e) { printf("[app] family load failed: %s\n", e.what()); }
        if (loader.Family != nullptr && hood != nullptr) FamilyName = hood->GetFamilyName((int16_t)loader.Family->ChunkID);
        for (auto& l : loader.Log) printf("[family] %s\n", l.c_str());
    }

    FSO::Files::LoadProgress::Step(lotTag + "view");
    // FreeSims UILotControl: the goto ghost used for "Go Here" pie menus on empty floor
    auto gotoGroup = _Ctx->CreateObjectInstance(GOTO_GUID, LotTilePos::OUT_OF_WORLD, Direction::NORTH, true, false);
    if (gotoGroup != nullptr && !gotoGroup->Objects.empty()) GotoObject = gotoGroup->Objects[0];

    auto arch = _Ctx->Architecture();
    auto& st = _World->State;
    st.Zoom(WorldZoom::Medium);
    st.Rotation(WorldRotation::TopLeft);
    st.Level(1);
    // centre on the house (walls of level 1), else the lot
    int minX = 9999, minY = 9999, maxX = -1, maxY = -1;
    auto& wl = arch->Walls[0];
    for (int y = 0; y < arch->Height; y++) for (int x = 0; x < arch->Width; x++)
        if ((int)wl[(size_t)(y * arch->Width + x)].Segments != 0) { minX = std::min(minX, x); minY = std::min(minY, y); maxX = std::max(maxX, x); maxY = std::max(maxY, y); }
    if (maxX >= 0) st.CenterTile(st::Vector2((minX + maxX + 1) / 2.0f, (minY + maxY + 1) / 2.0f));
    else st.CenterTile(st::Vector2(arch->Width / 2.0f, arch->Height / 2.0f));
    UpdateCutaway();

    if (!Family.empty()) Select(0);
    // settle: a few ticks so the Sims have their pose/outfit before the first frame
    Update(4);
    return true;
}

VMAvatar* LotSession::SelectedSim() {
    if (Selected < 0 || Selected >= (int)Family.size()) return nullptr;
    auto a = Family[(size_t)Selected];
    if (a == nullptr || a->Dead) return nullptr;
    return a;
}

void LotSession::Select(int index) {
    if (index < 0 || index >= (int)Family.size()) return;
    Selected = index;
    // TS1 global 3 "Selected Person ID" (Behavior.iff STR#129), as VMTS1FamilyLoader sets it
    if (_VM && _VM->GlobalState.size() > 3) _VM->GlobalState[3] = Family[(size_t)index]->ObjectID;
}

int LotSession::IndexOfFamily(VMEntity* e) const {
    for (size_t i = 0; i < Family.size(); i++) if (Family[i] == e) return (int)i;
    return -1;
}

void LotSession::Update(int n) {
    for (int i = 0; i < n; i++) {
        _VM->Update(); Updates++;
        // TS1(sims): objects lose value once a day (OBJD DailyDepreciation, down to DepreciationLimit), on the
        // object's current value (object data 41 CurrentValue, saved in the house file's OBJM records)
        int day = _Ctx->Clock->DayOfMonth;
        if (_LastDepreciationDay < 0) _LastDepreciationDay = day;
        else if (day != _LastDepreciationDay) { _LastDepreciationDay = day; DepreciateObjects(); }
    }
}

void LotSession::DepreciateObjects() {
    for (auto e : _VM->Entities) {
        auto go = dynamic_cast<VMGameObject*>(e);
        if (go == nullptr || go->MultitileGroup == nullptr || go->MultitileGroup->BaseObject() != go) continue;
        if (go->Position() == FSO::LotView::Model::LotTilePos::OUT_OF_WORLD) continue;
        auto def = go->MasterDefinition ? go->MasterDefinition : (go->Object ? go->Object->OBJ : nullptr);
        if (def == nullptr || def->DailyDepreciation == 0) continue;
        int value = go->GetValue(FSO::SimAntics::Model::VMStackObjectVariable::CurrentValue);
        if (value <= (int)def->DepreciationLimit) continue;
        value = std::max<int>((int)def->DepreciationLimit, value - (int)def->DailyDepreciation);
        go->SetValue(FSO::SimAntics::Model::VMStackObjectVariable::CurrentValue, (int16_t)value);
        if (go->MultitileGroup->Price > 0) go->MultitileGroup->Price = value;
    }
}

int LotSession::Budget() {
    using Engine::Utils::VMTS1Neighbourhood;
    if (!_VM) return 0;
    return _VM->GetGlobalValue(VMTS1Neighbourhood::GLOBAL_BUDGET_DIV) * 10000 + _VM->GetGlobalValue(VMTS1Neighbourhood::GLOBAL_BUDGET_MOD);
}

void LotSession::SetBudget(int value) {
    using Engine::Utils::VMTS1Neighbourhood;
    if (!_VM) return;
    if (value < 0) value = 0;
    _VM->SetGlobalValue(VMTS1Neighbourhood::GLOBAL_BUDGET_DIV, (int16_t)(value / 10000));
    _VM->SetGlobalValue(VMTS1Neighbourhood::GLOBAL_BUDGET_MOD, (int16_t)(value % 10000));
}

bool LotSession::RunArchitecture(std::vector<Model::VMArchitectureCommand>& commands, int& cost) {
    cost = 0;
    if (commands.empty()) return false;
    auto arch = _Ctx->Architecture();
    // VMNetArchitectureCmd.Execute sets CallerUID = the actor; RunCommands needs an avatar with that persist id
    VMEntity* actor = SelectedSim();
    if (actor == nullptr) for (auto e : _VM->Entities) if (dynamic_cast<VMAvatar*>(e)) { actor = e; break; }
    if (actor == nullptr) return false;
    for (auto& c : commands) c.CallerUID = actor->PersistID;
    // the price first (VMTSOGlobalLinkStub: the transaction must succeed before the commands run)
    cost = arch->SimulateCommands(commands, false);
    if (cost > Budget()) return false;
    arch->RunCommands(commands, false);
    if (getenv("ST_DEBUG_PLACE")) for (auto& c : commands) {
        auto f = arch->GetFloor((int16_t)c.x, (int16_t)c.y, c.level); auto w = arch->GetWall((int16_t)c.x, (int16_t)c.y, c.level);
        printf("[arch] cmd %d at %d,%d L%d x2 %d y2 %d pat %d style %d -> floor %d wallsegs %d\n", (int)c.Type, c.x, c.y, (int)c.level, c.x2, c.y2, c.pattern, c.style, f.Pattern, (int)w.Segments);
    }
    SetBudget(Budget() - cost);
    ArchitectureSpent += cost; // TS1(sims)
    arch->SignalRedraw();
    return true;
}

void LotSession::RespondDialog(int code, const std::string& text) {
    if (Dialogs.empty()) return;
    auto d = Dialogs.front();
    Dialogs.erase(Dialogs.begin());
    if (!d.Block) return;
    // VMNetDialogResponseCmd
    auto caller = _VM->GetObjectById(d.CallerID);
    if (caller == nullptr || caller->Thread == nullptr || caller->Thread->BlockingState == nullptr) return;
    if (caller->Thread->BlockingState->StateType() != Model::TSOPlatform::VMAsyncStateType::DialogResult) return;
    auto state = std::static_pointer_cast<Primitives::VMDialogResult>(caller->Thread->BlockingState);
    state->Responded = true;
    state->ResponseCode = (uint8_t)code;
    state->ResponseText = text;
}

void LotSession::Draw() {
    _World->Draw(Frame);
}

void LotSession::Rotate(int dir) {
    auto& st = _World->State;
    int r = ((int)st.Rotation() + (dir > 0 ? 1 : 3)) & 3;
    auto center = st.CenterTile();
    st.Rotation((WorldRotation)r);
    st.CenterTile(center);
    UpdateCutaway();
}

void LotSession::Zoom(int dir) {
    auto& st = _World->State;
    int z = (int)st.Zoom() + (dir > 0 ? 1 : -1); // Far = 1, Medium = 2, Near = 3
    if (z < 1 || z > 3) return;
    st.Zoom((WorldZoom)z);
    // TS1(sims): sprite memory. An object's SPR2 holds the frames of all three zooms and decoded frames stay in
    // memory (near ones are 4x the medium ones: +17 MB on a full house). The zoom change above cleared the world's
    // texture cache and invalidated every object renderer, so no decoded pixels are referenced right now: drop them
    // all; the next frame decodes only what the new zoom shows.
    auto content = FSO::Content::Content::Get();
    if (content && content->WorldObjects) content->WorldObjects->DropDecodedSprites();
    // FIX(sims) memory: the wall/floor frames (SPR#, walls.iff/floors.iff/*.wll/*.flr) of the old zoom as well
    FSO::Files::Formats::IFF::IffFile::DropDecodedFrames();
}

void LotSession::ChangeLevel(int dir) {
    auto& st = _World->State;
    int l = st.Level() + dir;
    auto arch = _Ctx->Architecture();
    if (l < 1 || l > (int)arch->Stories || (size_t)l > arch->Floors.size()) return;
    if (dir > 0) { // only go up if something is built there
        bool any = false;
        for (auto& f : arch->Floors[(size_t)(l - 1)]) if (f.Pattern != 0) { any = true; break; }
        if (!any) for (auto& w : arch->Walls[(size_t)(l - 1)]) if ((int)w.Segments != 0) { any = true; break; }
        if (!any) return;
    }
    st.Level((int8_t)l);
    UpdateCutaway();
}

void LotSession::ScrollScreen(float dxPx, float dyPx) {
    // World::Scroll moves by basis vectors (FreeSims: GetScrollBasis); one unit of basis[0] is
    // TilePxWidth * multiplier pixels across, basis[1] the same distance down.
    auto& ws = _World->State.WorldSpace;
    int multiplier = ((1 << (3 - (int)_World->State.Zoom())) * 3) / 2;
    float unitX = ws.TilePxWidth * multiplier;       // pixels per basis[0] unit
    float unitY = ws.TilePxHeight * multiplier * 2;  // pixels per basis[1] unit ((2,2) tiles down)
    if (unitX <= 0 || unitY <= 0) return;
    _World->Scroll(st::Vector2(dxPx / unitX, dyPx / unitY));
}

void LotSession::CenterOn(VMEntity* e) {
    if (e == nullptr || e->Position() == LotTilePos::OUT_OF_WORLD) return;
    auto p = e->Position();
    _World->State.CenterTile(st::Vector2(p.x / 16.0f, p.y / 16.0f));
    if (p.Level != _World->State.Level()) { _World->State.Level(p.Level); UpdateCutaway(); }
}

void LotSession::SetWallsMode(int mode) { WallsMode = mode % 3; UpdateCutaway(); }

void LotSession::UpdateCutaway() {
    auto arch = _Ctx->Architecture();
    auto& st = _World->State;
    if (WallsMode == 1) {
        Blueprint->Cutaway.assign((size_t)(arch->Width * arch->Height), true);
        st.DynamicCutaway = false;
    } else if (WallsMode == 0) {
        std::unordered_set<uint32_t> rooms;
        for (size_t i = 1; i < _Ctx->RoomInfo.size(); i++) if (!_Ctx->RoomInfo[i].Room.IsOutside) rooms.insert((uint32_t)i);
        _RoomCuts = Utils::VMArchitectureTools::GenerateRoomCut(arch, st.Level(), st.Rotation(), rooms);
        st.DynamicCutaway = true;
        ApplyMouseCut(true);
    } else {
        Blueprint->Cutaway.assign((size_t)(arch->Width * arch->Height), false);
        st.DynamicCutaway = false;
    }
    // HOOK(world roofs): TS1 shows the roof with walls up when the top story is in view (the original's "Roof"
    // view); never with walls cutaway/down or while a lower story is selected.
    {
        int top = 1;
        for (int l = 1; l <= (int)arch->Floors.size(); l++)
            for (auto& f : arch->Floors[(size_t)(l - 1)]) if (f.Pattern != 0) { top = std::max(top, l); break; }
        st.DrawRoofs = (WallsMode == 2) && st.Level() >= top;
    }
    Blueprint->Damage.push_back(FSO::LotView::Model::BlueprintDamage(FSO::LotView::Model::BlueprintDamageType::WALL_CUT_CHANGED));
}

void LotSession::UpdateMouseCut(bool shown, int screenX, int screenY) {
    if (WallsMode != 0 || _World == nullptr || Blueprint == nullptr) return;
    auto& st = _World->State;
    st::Rectangle newCut; // C#: an empty rectangle while there is no mouse (touch only)
    if (shown && CursorCutaway) {
        auto mouseTilePos = st.WorldSpace.GetTileAtPosWithScroll(st::Vector2((float)screenX, (float)screenY));
        newCut = st::Rectangle((int)(mouseTilePos.X - 2.5), (int)(mouseTilePos.Y - 2.5), 5, 5);
        newCut.X -= Utils::VMArchitectureTools::CutCheckDir[(int)st.Rotation()][0] * 2;
        newCut.Y -= Utils::VMArchitectureTools::CutCheckDir[(int)st.Rotation()][1] * 2;
    }
    if (newCut.X == _MouseCutRect.X && newCut.Y == _MouseCutRect.Y && newCut.Width == _MouseCutRect.Width
        && newCut.Height == _MouseCutRect.Height) return;
    _MouseCutRect = newCut;
    ApplyMouseCut(false);
}

void LotSession::ApplyMouseCut(bool force) {
    // C#: finalCut = copy of LastCuts + ApplyCutRectangle; damage only if the room cut changed, or the rectangle
    // cut (now or last time) touched a wall.
    auto arch = _Ctx->Architecture();
    if (_RoomCuts.size() != (size_t)(arch->Width * arch->Height)) return;
    auto finalCut = _RoomCuts;
    bool notableChange = (_MouseCutRect.Width > 0)
        && Utils::VMArchitectureTools::ApplyCutRectangle(arch, _World->State.Level(), finalCut, _MouseCutRect);
    if (force || notableChange || _LastRectCutNotable) {
        Blueprint->Cutaway = finalCut;
        if (!force) Blueprint->Damage.push_back(FSO::LotView::Model::BlueprintDamage(FSO::LotView::Model::BlueprintDamageType::WALL_CUT_CHANGED));
    }
    _LastRectCutNotable = notableChange;
}

static void ToChoices(const std::vector<VMPieMenuInteraction>& menu, std::vector<PieChoice>& pie) {
    pie.clear();
    for (auto& m : menu) {
        PieChoice c;
        c.Name = m.Name.empty() ? std::string("???") : m.Name;
        c.ID = m.ID;
        c.Param0 = m.Param0;
        pie.push_back(c);
    }
}

VMEntity* LotSession::PieTargetAt(int x, int y, std::vector<PieChoice>& pie, bool& isGoto, int16_t& pickedId) {
    pie.clear();
    isGoto = false;
    pickedId = 0;
    auto active = SelectedSim();
    if (active == nullptr) return nullptr;
    auto hit = PickObjectAt(*_World, &Frame, x, y, 3);
    pickedId = hit.ObjectID;
    VMEntity* obj = nullptr;
    if (hit.ObjectID > 0) {
        obj = _VM->GetObjectById(hit.ObjectID);
        if (obj != nullptr && obj->MultitileGroup != nullptr) obj = obj->MultitileGroup->GetInteractionGroupLeader(obj);
        if (obj != nullptr) {
            ToChoices(obj->GetPieMenu(_VM.get(), active, false), pie);
            if (!pie.empty() || dynamic_cast<VMAvatar*>(obj) != nullptr) return obj;
        }
    }
    // empty floor (or an object without interactions): the goto object on that tile, if walkable
    if (GotoObject == nullptr) return obj;
    auto tile = _World->State.WorldSpace.GetTileAtPosWithScroll(st::Vector2((float)x, (float)y));
    LotTilePos targetPos = LotTilePos::FromBigTile((int16_t)std::floor(tile.X), (int16_t)std::floor(tile.Y), _World->State.Level());
    if (_Ctx->IsOutOfBounds(targetPos) || _Ctx->SolidToAvatars(targetPos).Solid) targetPos = LotTilePos::OUT_OF_WORLD;
    GotoObject->SetPosition(targetPos, Direction::NORTH, _Ctx.get());
    if (GotoObject->Position() == LotTilePos::OUT_OF_WORLD) return obj;
    if (obj != nullptr && hit.ObjectID > 0) return obj; // tapped an object with no interactions: no "go here" (FreeSims: error tooltip)
    ToChoices(GotoObject->GetPieMenu(_VM.get(), active, false), pie);
    isGoto = true;
    return GotoObject;
}

bool LotSession::PushInteraction(VMEntity* target, bool isGoto, const PieChoice& choice) {
    auto caller = SelectedSim();
    if (caller == nullptr || target == nullptr || caller->Thread == nullptr) return false;
    if ((int)caller->Thread->Queue.size() >= Engine::VMThread::MAX_USER_ACTIONS) return false;
    if (isGoto) {
        // VMNetGotoCmd: a fresh goto object at the tile, the interaction is pushed on it
        auto pos = target->Position();
        auto group = _Ctx->CreateObjectInstance(GOTO_GUID, pos, Direction::NORTH, false, false);
        VMEntity* callee = (group == nullptr || group->Objects.empty()) ? nullptr : group->Objects[0];
        if (callee == nullptr || callee->Position() == LotTilePos::OUT_OF_WORLD) return false;
        auto before = caller->Thread->Queue;
        callee->PushUserInteraction(choice.ID, caller, _Ctx.get());
        for (auto& q : caller->Thread->Queue) if (std::find(before.begin(), before.end(), q) == before.end()) q->UserQueued = true; // FIX(sims)
        return true;
    }
    // VMNetInteractionCmd
    std::vector<int16_t> args = { choice.Param0, 0, 0, 0 };
    auto before = caller->Thread->Queue;
    target->PushUserInteraction(choice.ID, caller, _Ctx.get(), &args);
    // FIX(sims): mark the player's action (VMThread::EnqueueAction keeps it when a higher-priority push queue-skips)
    for (auto& q : caller->Thread->Queue) if (std::find(before.begin(), before.end(), q) == before.end()) q->UserQueued = true;
    return true;
}

void LotSession::CancelAction(uint16_t uid) {
    auto caller = SelectedSim();
    if (caller == nullptr || caller->Thread == nullptr) return;
    caller->Thread->CancelAction(uid); // VMNetInteractionCancelCmd
}

bool LotSession::EntityScreenPos(VMEntity* e, float& sx, float& sy, float heightTiles) {
    if (e == nullptr || e->WorldUI == nullptr) return false;
    auto p = e->WorldUI->DrawPosition();
    if (p.X < -2043 && p.Y < -2043) return false;
    st::Vector3 tile(p.X, p.Y, p.Z + heightTiles);
    if (dynamic_cast<VMGameObject*>(e) != nullptr) tile = st::Vector3(p.X + 0.5f, p.Y + 0.5f, p.Z + heightTiles);
    auto s = TileToScreen(*_World, tile);
    sx = s.X; sy = s.Y;
    return true;
}

}
