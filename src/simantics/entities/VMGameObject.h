/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
// FSO.SimAntics.VMGameObject (entities/VMGameObject.cs)
#pragma once
#include "simantics/entities/VMEntity.h"
namespace FSO { namespace SimAntics {

enum class VMGameObjectDisableFlags : int32_t {
    TransactionIncomplete = 1 << 0,
    ForSale = 1 << 1,
    //past this point disabled objects appear in grayscale.
    LotCategoryWrong = 1 << 2,
    ObjectLimitExceeded = 1 << 3, //when too many objects are on a lot and the object lot is lowered, the last few objects are disabled.
    PendingRoommateDeletion = 1 << 4,
    ObjectLimitThreadDisable = 1 << 5 //activated when object limit exceeded and object is no longer in use.
};
FSO_FLAGS_ENUM(VMGameObjectDisableFlags)
inline bool operator>=(VMGameObjectDisableFlags a, VMGameObjectDisableFlags b) { return (int32_t)a >= (int32_t)b; }

class VMGameObject : public VMEntity {
public:
    VMGameObjectDisableFlags Disabled = (VMGameObjectDisableFlags)0;

    /** Definition **/ // worldUI ownership passes to the entity
    VMGameObject(FSO::Content::GameObject* def, FSO::LotView::Components::ObjectComponent* worldUI);

    void SetDynamicSpriteFlag(uint16_t index, bool set) override;
    bool SetValue(Model::VMStackObjectVariable var, int16_t value) override;
    int16_t GetValue(Model::VMStackObjectVariable var) override;
    bool RefreshGraphic();
    void SetRoom(uint16_t room) override;
    void RefreshLight();
    void Init(VMContext* context) override;

    float RadianDirection() const override;
    void RadianDirection(float value) override;
    DirectionT Direction() const override { return _Direction; }
    void Direction(DirectionT value) override;
    bool PartOfPortal();
    st::Vector3 VisualPosition() const override;
    void VisualPosition(const st::Vector3& value) override;
    std::string ToString() override;
    void DisableIfTSOCategoryWrong(VMContext* context);

    // Begin Container SLOTs interface
    int32_t TotalSlots() override;
    bool PlaceInSlot(VMEntity* obj, int32_t slot, bool cleanOld, VMContext* context) override;
    VMEntity* GetSlot(int32_t slot) override;
    int32_t GetSlotHeight(int32_t slot) override;
    void ClearSlot(int32_t slot) override;
    // End Container SLOTs interface

    void* GetIcon(void* gd, int32_t store) override; // STUB(3ds)
    void PrePositionChange(VMContext* context) override;
    std::unique_ptr<Model::Routing::VMObstacle> GetObstacle(const LotTilePos& pos, DirectionT dir) override;
    void PositionChange(VMContext* context, bool noEntryPoint) override;
private:
    DirectionT _Direction = DirectionT::NORTH;
    FSO::LotView::Components::ObjectComponent* ObjUI() const { return (FSO::LotView::Components::ObjectComponent*)WorldUI.get(); }
};
}}
