/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
// TS1(sims): see VMTS1MiscPrimitives.h. Burn and Show String are translated from FreeSO (MPL-2.0).
#include "simantics/primitives/VMTS1MiscPrimitives.h"
#include "simantics/engine/VMStackFrame.h"
#include "simantics/engine/VMSimanticsException.h"
#include "simantics/engine/VMThread.h"
#include "simantics/engine/VMDialogHandler.h"
#include "simantics/entities/VMEntity.h"
#include "simantics/entities/VMAvatar.h"
#include "simantics/entities/VMGameObject.h"
#include "simantics/entities/VMMultitileGroup.h"
#include "simantics/model/VMMotive.h"
#include "simantics/model/VMPersonDataVariable.h"
#include "simantics/model/VMStackObjectVariable.h"
#include "simantics/VM.h"
#include "simantics/VMContext.h"
#include "simantics/VMArchitecture.h"
#include "files/formats/iff/chunks/STR.h"
#include "files/formats/iff/chunks/OBJD.h"
#include <algorithm>
#include <deque>

namespace FSO { namespace SimAntics { namespace Primitives {

using namespace FSO::SimAntics::Engine;
using FSO::LotView::Model::LotTilePos;
using FSO::LotView::Model::Direction;
using FSO::Files::Formats::IFF::Chunks::STR;

VMPrimitiveExitCode VMBurn::Execute(VMStackFrame* context, VMPrimitiveOperand* args)
{
    auto operand = static_cast<VMBurnOperand*>(args);
    auto vm = context->VM();
    auto ctx = vm->Context;
    if (context->StackObject == nullptr) return VMPrimitiveExitCode::GOTO_FALSE;
    bool isSpread = operand->Type != VMBurnType::StackObject;

    if (isSpread && (int32_t)ctx->NextRandom(10000) >= ctx->Clock->FirePercent) return VMPrimitiveExitCode::GOTO_FALSE;

    auto myRoom = ctx->GetObjectRoom(context->Caller);
    if (myRoom < ctx->RoomInfo.size() && ctx->RoomInfo[myRoom].Room.IsPool) return VMPrimitiveExitCode::GOTO_FALSE;

    //as fires burn, the chance they can spread lowers dramatically.
    if (isSpread) ctx->Clock->FirePercent -= 500; // lower by 5% every spread. 40 spreads reaches 0.
    if (ctx->Clock->FirePercent < 0) ctx->Clock->FirePercent = 0;

    auto arch = ctx->Architecture();
    auto query = ctx->ObjectQueries.get();
    if (arch == nullptr || query == nullptr) return VMPrimitiveExitCode::GOTO_FALSE;
    std::vector<bool> fires((size_t)(arch->Width * arch->Height), false);
    std::deque<LotTilePos> spread;

    if (context->StackObject == nullptr) throw VMSimanticsException("Object reference not set to an instance of an object.", context); // FIX(sims) QA4: was a null dereference
    LotTilePos pos = context->StackObject->Position();
    if (operand->Type == VMBurnType::FrontOfStackObject) {
        switch (context->StackObject->Direction()) {
            case Direction::SOUTH: pos.y += 16; break;
            case Direction::WEST: pos.x -= 16; break;
            case Direction::EAST: pos.x += 16; break;
            case Direction::NORTH: pos.y -= 16; break;
            default: break;
        }
    }
    if (pos == LotTilePos::OUT_OF_WORLD) return VMPrimitiveExitCode::GOTO_FALSE;

    auto hasFire = [&](const LotTilePos& p) {
        auto objs = query->GetObjectsAt(p);
        if (objs == nullptr) return false;
        for (auto o : *objs) if (o->Object->OBJ->GUID == FIRE_GUID) return true;
        return false;
    };
    auto tileIndex = [&](const LotTilePos& p) -> int { int i = p.TileX() + p.TileY() * arch->Width; return (i >= 0 && i < (int)fires.size()) ? i : -1; };

    bool first = true;
    bool madeAFire = false;
    spread.push_back(pos);
    while (!spread.empty()) {
        auto item = spread.front(); spread.pop_front();
        if (item == LotTilePos::OUT_OF_WORLD) continue;
        if (first && !hasFire(item)) {
            madeAFire = true;
            ctx->CreateObjectInstance(FIRE_GUID, LotTilePos::FromBigTile(pos.TileX(), pos.TileY(), pos.Level), Direction::NORTH, true);
        }
        first = false;
        auto objp = query->GetObjectsAt(item);
        if (objp == nullptr) continue;
        std::vector<VMEntity*> objat(*objp); // the list changes as fires are created
        for (auto obj : objat) {
            auto ava = dynamic_cast<VMAvatar*>(obj);
            bool inUse = obj->IsInUse(ctx, true) || (ava != nullptr && ava->GetPersonData(Model::VMPersonDataVariable::IsGhost) > 0);
            if (inUse && !operand->BurnBusyObjects) continue;
            if (obj->MultitileGroup == nullptr) continue;
            for (auto sobj : obj->MultitileGroup->Objects) {
                //is this object burnable? if so, set it as burning
                if (sobj->Position().Level != pos.Level) continue;
                if ((sobj->GetValue(Model::VMStackObjectVariable::FlagField2) & (int16_t)VMEntityFlags2::Burns) != 0 && !sobj->GetFlag(VMEntityFlags::FireProof)) {
                    sobj->SetFlag(VMEntityFlags::Burning, true);
                    auto fpos = sobj->Position();
                    int index = tileIndex(fpos);
                    if (index < 0) continue;
                    if (fires[(size_t)index] || hasFire(fpos)) { fires[(size_t)index] = true; continue; }
                    spread.push_back(fpos);
                    madeAFire = true;
                    ctx->CreateObjectInstance(FIRE_GUID, LotTilePos::FromBigTile(fpos.TileX(), fpos.TileY(), fpos.Level), Direction::NORTH, true);
                    fires[(size_t)index] = true;
                }
            }
        }
    }
    return madeAFire ? VMPrimitiveExitCode::GOTO_TRUE : VMPrimitiveExitCode::GOTO_FALSE;
}

VMPrimitiveExitCode VMShowString::Execute(VMStackFrame* context, VMPrimitiveOperand* args)
{
    if (dynamic_cast<VMGameObject*>(context->StackObject) != nullptr) return VMPrimitiveExitCode::GOTO_TRUE;
    auto operand = static_cast<VMShowStringOperand*>(args);
    auto table = context->ScopeResource()->Get<STR>(operand->StringTable);
    auto avatar = dynamic_cast<VMAvatar*>(context->StackObject);
    if (table != nullptr && avatar != nullptr) {
        auto message = VMDialogHandler::ParseDialogString(context, table->GetString(operand->StringID - 1), table);
        avatar->Message(message);
    }
    return VMPrimitiveExitCode::GOTO_TRUE;
}

VMPrimitiveExitCode VMFind5WorstMotives::Execute(VMStackFrame* context, VMPrimitiveOperand* args)
{
    (void)args;
    auto avatar = dynamic_cast<VMAvatar*>(context->Caller);
    if (avatar == nullptr) return VMPrimitiveExitCode::GOTO_FALSE;
    static const int motives[8] = { 5, 6, 7, 8, 9, 13, 14, 15 }; // energy comfort hunger hygiene bladder room social fun
    std::vector<std::pair<int16_t, int>> v;
    for (int m : motives) v.push_back({ avatar->GetMotiveData((Model::VMMotive)m), m });
    std::stable_sort(v.begin(), v.end(), [](const std::pair<int16_t, int>& a, const std::pair<int16_t, int>& b) { return a.first < b.first; });
    for (size_t i = 0; i < 5 && i < context->Thread->TempRegisters.size(); i++) context->Thread->TempRegisters[i] = (int16_t)v[i].second;
    return VMPrimitiveExitCode::GOTO_TRUE;
}

}}}
