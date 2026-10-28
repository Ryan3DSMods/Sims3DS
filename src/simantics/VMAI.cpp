/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
// TSO.SimsAntics.VMFreeWill (VMAI.cs)
#include "simantics/VMAI.h"
#include "simantics/VM.h"
#include "simantics/VMContext.h"
#include "simantics/entities/VMAvatar.h"
#include "simantics/engine/VMThread.h"
#include "simantics/engine/VMQueuedAction.h"
#include <cstdlib>
namespace FSO { namespace SimAntics {
using namespace FSO::Files::Formats::IFF::Chunks;
using FSO::LotView::Model::LotTilePos;

int32_t VMFreeWill::RandNext(int32_t min, int32_t max) {
    if (max <= min) return min;
    return min + (rand() % (max - min));
}

VMFreeWill::VMFreeWill(FSO::SimAntics::VM* vm) {
    VM = vm;
    Entities = vm->Entities;
    for (auto ent : Entities) {
        auto ava = dynamic_cast<VMAvatar*>(ent);
        if (ava != nullptr && ava->Visitor)
            Visitors.push_back(ava);
    }
}

void VMFreeWill::CheckForUsableObjects(FSO::SimAntics::VM* vm) {
    Visitors.clear();
    Entities = vm->Entities;
    if (Entities.size() > 0)
        for (auto ent : Entities) {
            if (ent->Position() != LotTilePos::OUT_OF_WORLD && ent->GetBadNames() == false
                && ent->Object->GUID != VMAvatar::TEMPLATE_PERSON && ent->Object->GUID != VMAvatar::TEMPLATE_NPC)
                Objects.push_back(ent);
        }

    for (auto ent : Entities) {
        auto ava = dynamic_cast<VMAvatar*>(ent);
        if (ava != nullptr)
            Visitors.push_back(ava);
    }
}

void VMFreeWill::SetSelected(VMEntity* entity, bool isPet) {
    interactionList.clear();
    if (entity->TreeTable != nullptr && entity->TreeTableStrings != nullptr) {
        TreeTableSelected = entity->TreeTable;
        if (isPet) {
            for (auto& interaction : entity->TreeTable->Interactions) {
                if (interaction->Flags == TTABFlags::TS1AllowDogs || interaction->Flags == TTABFlags::TS1AllowCats)
                    interactionList.push_back(entity->TreeTableStrings->GetString((int32_t)interaction->TTAIndex));
            }
        } else {
            for (auto& interaction : entity->TreeTable->Interactions) {
                if (!interaction->Debug() || !interaction->WhenDead() || !interaction->Leapfrog()
                    || !interaction->AllowGhosts() || !interaction->AllowCSRs() || !interaction->AllowConsecutive()
                    || !interaction->AllowDogs() || !interaction->AllowCats())
                    interactionList.push_back(entity->TreeTableStrings->GetString((int32_t)interaction->TTAIndex));
            }
        }
    }
}

void VMFreeWill::ExecuteAction(VMEntity* avatar, VMEntity* target) {
    int id = 0;
    if (interactionList.size() > 0 && TreeTableSelected != nullptr) {
        id = RandNext(0, (int32_t)interactionList.size() - 1);
        if (id >= (int)TreeTableSelected->Interactions.size()) return;
        auto interaction = TreeTableSelected->Interactions[id].get();
        auto ActionID = interaction->ActionFunction;
        BHAV* bhav;
        if (ActionID < 4096) { //global
            bhav = nullptr;
            //unimp as it has to access the context to get this.
        } else if (ActionID < 8192) { //local
            bhav = target->Object->Resource->Get<BHAV>(ActionID);
        } else { //semi-global
            bhav = (target->SemiGlobal == nullptr) ? nullptr : target->SemiGlobal->Get<BHAV>(ActionID);
        }

        if (bhav != nullptr) {
            auto action = std::make_shared<Engine::VMQueuedAction>();
            action->ActionRoutine = VM->Assemble(bhav);
            action->Callee = target;
            action->StackObject = target;
            action->CodeOwner = target->Object;
            action->InteractionNumber = (int32_t)interaction->TTAIndex; //interactions are referenced by their tta index
            action->Priority = (int16_t)Engine::VMQueuePriority::UserDriven;
            avatar->Thread->EnqueueAction(action);
        }
    }
}

void VMFreeWill::RunAction(VMEntity* entity) {
    bool PetAction = entity->Object->GUID == VMAvatar::DOG_TEMPLATE ||
        entity->Object->GUID == VMAvatar::CAT_TEMPLATE ? true : false;

    if (Objects.size() > 0) {
        int n = RandNext(0, (int32_t)Objects.size() - 1);
        Target = Objects[n];
        Objects.erase(Objects.begin() + n);
        SetSelected(Target, PetAction);
        if (entity != Target)
            ExecuteAction(entity, Target);
    }
}

void VMFreeWill::Tick(FSO::SimAntics::VM* vm) {
    Objects.clear();
    CheckForUsableObjects(vm);

    for (size_t i = 0; i < Visitors.size(); i++)
        if (Visitors[i]->Visitor && Visitors[i]->Thread != nullptr) {
            if (Visitors[i]->Thread->Queue.size() > 2)
                Visitors[i]->Thread->Queue.clear();

            if (Visitors[i]->Thread->Queue.size() < 2 ||
                Visitors[i]->Thread->Queue[0]->Priority == (int16_t)Engine::VMQueuePriority::Idle ||
                Visitors[i]->Thread->Queue[1]->Priority == (int16_t)Engine::VMQueuePriority::Idle) {
                RunAction(Visitors[i]);
            }
        }
}
}}
