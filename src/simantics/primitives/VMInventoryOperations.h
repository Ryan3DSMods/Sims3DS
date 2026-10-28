/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
// FSO.SimAntics.Primitives.VMInventoryOperations / VMInventoryOperationsOperand / VMInventoryOpMode /
// VMInventoryRestoreType / VMInventoryOpState (primitives/VMInventoryOperations.cs)
#pragma once
#include <cstdint>
#include "simantics/engine/VMPrimitiveHandler.h"
#include "simantics/engine/VMPrimitiveOperand.h"
#include "simantics/engine/scopes/VMVariableScope.h"
#include "simantics/model/TSOPlatform/VMAsyncState.h"

namespace FSO { namespace SimAntics { namespace Primitives {

//inventory operations for TSO
class VMInventoryOperations : public Engine::VMPrimitiveHandler {
public:
    Engine::VMPrimitiveExitCode Execute(Engine::VMStackFrame* context, Engine::VMPrimitiveOperand* args) override;
};

enum class VMInventoryOpMode : uint8_t
{
    GetRemainingCapacity = 0, //result in temp 0
    AddStackObjToInventory = 1, //true/false. Original game doesn't expect false?
    RemoveTemp0ObjOfTypeFromInventory = 2, //true/false. Original game doesnt expect false?
    CountObjectsOfType = 3, //result in temp 0

    FSOCreateObjectOfTypeOOW = 4, // scope/data is index to access. if index is out of range, goes to last object. if no object, returns false. id in temp 0
    FSOCopyObjectOfTypeOOW = 5, // same as above, but does not claim the object. id in temp 0.
    FSOSaveStackObj = 6, //same as add, but does not delete the object afterwards
    FSOCountAllObjectsOfType = 7, //includes placed objects
};

class VMInventoryOperationsOperand : public Engine::VMPrimitiveOperand {
public:
    uint32_t GUID = 0;
    VMInventoryOpMode Mode = VMInventoryOpMode::GetRemainingCapacity;

    // #region VMPrimitiveOperand Members
    void Read(const uint8_t* bytes) override;
    void Write(uint8_t* bytes) override;
    // #endregion
};

enum class VMInventoryRestoreType : uint8_t
{
    Normal,
    CreateOOW,
    CopyOOW
};

class VMInventoryOpState : public Model::TSOPlatform::VMAsyncState {
public:
    bool Success = false;
    bool WriteTemp0 = false;
    int16_t Temp0Value = 0;
    uint32_t ObjectPersistID = 0;
    Engine::Scopes::VMVariableScope WriteScope = Engine::Scopes::VMVariableScope::INVALID;
    int16_t WriteData = 0;

    Model::TSOPlatform::VMAsyncStateType StateType() const override { return Model::TSOPlatform::VMAsyncStateType::InventoryOpState; }

    // Deserialize(BinaryReader) / SerializeInto(BinaryWriter) dropped, as for the core VMAsyncState
    // (no save-state marshalling on 3DS). C# Deserialize: Success, WriteTemp0, Temp0Value; if Version > 34 also
    // ObjectPersistID, WriteScope (byte), WriteData; else if WriteTemp0 { WriteScope = Temps; WriteData = 0; }.
    // SerializeInto wrote Success, WriteTemp0, Temp0Value.
};

}}}
