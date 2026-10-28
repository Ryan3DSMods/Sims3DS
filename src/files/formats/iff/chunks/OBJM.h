/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
// Translated from FreeSims sims.files/formats/iff/chunks/OBJM.cs, replaced by the decoded format from FreeSO
// tso.files/Formats/IFF/Chunks/OBJM.cs (TS1(sims)). The FreeSims "MappedObject" heuristic (search for the birth
// year 1997) is kept only as a fallback when the per-object records cannot be decoded.
#pragma once
#include "files/formats/iff/AbstractIffChunk.h"
#include "files/formats/iff/chunks/OBJD.h"
#include "files/formats/iff/chunks/OBJT.h"
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace FSO { namespace Files { namespace Formats { namespace IFF { namespace Chunks {

class IffFieldEncode;

// ---- TS1(sims): FreeSO OBJM record types ----

/// Full tile offsets of a multitile part within a group, along with the ID of the lead object.
struct OBJMMultitile {
    // Relative tile offset to the lead tile in world space.
    int32_t OffsetX = 0, OffsetY = 0, OffsetLevel = 0;
    // Absolute tile position in group space (the same regardless of rotation). Always positive, doesn't care what tile is the lead.
    int32_t GroupX = 0, GroupY = 0, GroupLevel = 0;
    /// If this is 0, this object is the lead.
    int16_t MultitileParentID = 0;
    void Read(IffFieldEncode& iop);
};

struct OBJMFootprint {
    int32_t MinY = 0, MinX = 0, MaxY = 0, MaxX = 0;
    void Read(IffFieldEncode& iop);
};

struct OBJMMotiveDelta {
    int32_t Motive = 0;
    float TickDelta = 0; // 30 ticks per minute, 60 minutes per hour
    float StopAt = 0;
    void Read(IffFieldEncode& iop);
};

enum class OBJMInteractionFlags : int32_t {
    AutoFirst = 1,
    PushHeadContinuation = 2,
    UserInitiated = 4, // Interaction is not autonomous. Can be 0 for primitive pushed interactions until they actually run.
    CanBeAuto = 8, // check tree succeeds with param 0 == 1
    Unknown16 = 16, // Something to do with interaction push
    Completed = 32,
    CarryNameOver = 64,
    Unknown128 = 128, // Something to do with interaction push
    UserInterrupted = 256 // When the interaction has the X over it.
};

enum class OBJMRoutingState : int32_t { None = 0, Stopped = 3, Turning = 4, Accelerating = 6, Walking = 9 };

struct OBJMInteraction {
    int32_t UID = 0;
    int16_t CallerID = 0;
    int16_t TargetID = 0;
    int16_t Icon = 0;
    int32_t TTAIndex = -1;
    int16_t Args[4] = { 0, 0, 0, 0 };
    int32_t Priority = 0;
    int16_t ActionTreeID = 0;
    float Attenuation = 0;
    int32_t Flags = 0; // OBJMInteractionFlags
    void Read(IffFieldEncode& iop);
    bool IsValid() const { return TTAIndex != -1; }
};

struct OBJMAccessory {
    std::string Name;
    std::string Binding;
    void Read(IffFieldEncode& iop);
};

struct OBJMObjectUse {
    int16_t TargetID = 0;
    int32_t StackLength = 0; // If the stack length falls below this, then the object is no longer in use.
    uint8_t Unknown2 = 0; // 1 when call functional tree?
    void Read(IffFieldEncode& iop);
};

struct OBJMPerson {
    int32_t AnimEventCount = 0; // Number of events fired during the current animation.
    int32_t Engaged = 0; // 0 when routing or waiting for notify (can be interrupted), 1 otherwise.
    std::string Body, BodyTex, Unk1, Unk2, LeftHand, LeftHandTex, RightHand, RightHandTex, Head, HeadTex;
    std::vector<OBJMAccessory> Accessories;
    // name;priority;speed (1/1000ths);frame;weight? (1/1000ths);loop;unk;unk
    std::string Animation, CarryAnimation, BaseAnimation;
    OBJMRoutingState RoutingState = OBJMRoutingState::None;
    float FirstFloats[9] = {};
    float MotiveDataOld[16] = {};
    float MotiveData[16] = {};
    std::vector<int16_t> PersonData;
    int32_t RoutingFrameCount = 0;
    OBJMInteraction ActiveInteraction;
    OBJMInteraction LastInteraction;
    std::vector<OBJMInteraction> InteractionQueue;
    std::vector<OBJMObjectUse> ObjectUses;
    std::vector<OBJMMotiveDelta> MotiveDeltas;
    void Read(uint32_t version, IffFieldEncode& iop);
};

struct OBJMSlot {
    int16_t Unknown = 0;
    int16_t ObjectID = 0;
};

struct OBJMResource {
    Chunks::OBJD* OBJD = nullptr;
    Chunks::OBJTEntry* OBJT = nullptr;
};

struct OBJMStackFrame {
    int16_t StackObjectID = 0;
    int16_t TreeID = 0;
    int16_t NodeID = 0;
    std::vector<int16_t> Parameters;
    std::vector<int16_t> Locals;
    int32_t PrimitiveState = 0;
    int16_t CodeOwnerObjType = 0; // object type that owns the running code
    void Read(IffFieldEncode& iop);
};

struct OBJMRelationshipEntry {
    int32_t IsPresent = 0;
    int32_t TargetID = 0;
    std::vector<int32_t> Values;
    void Read(IffFieldEncode& iop);
};

struct OBJMInstance {
    static const int TempCount = 8;
    static const int ObjectDataCount = 68;
    static const int ExtraDataCount = 5;

    Chunks::OBJD* OBJD = nullptr;
    Chunks::OBJTEntry* OBJT = nullptr;

    OBJMFootprint Footprint;
    int32_t X = 0; // 1/16 tile units (LotTilePos x/y); -16,-16 = out of world
    int32_t Y = 0;
    int32_t Level = 0;
    int16_t UnknownData = 0;

    std::vector<int16_t> Attributes;
    std::vector<int16_t> TempRegisters;
    std::vector<int16_t> ObjectData;
    std::vector<int16_t> ObjectDataExtra;

    int32_t StackFlags = 0; //512 usually
    std::vector<OBJMStackFrame> Stack;
    std::vector<OBJMRelationshipEntry> Relationships;

    /// Each tile has a linked list of objects on the same tile - this slot points to the next object.
    OBJMSlot LinkedSlot;
    /// A pair for each object container slot. Second field appears to be the contained object ID.
    std::vector<OBJMSlot> Slots;
    std::vector<int16_t> DynamicSpriteFlags;

    bool HasMultitileData = false;
    OBJMMultitile MultitileData;
    std::vector<float> PortalData;
    bool HasPersonData = false;
    OBJMPerson PersonData;

    int32_t UnknownInt = 0;

    /// Throws std::runtime_error on an unexpected layout (FreeSO throws the same cases).
    void Read(uint32_t version, IffFieldEncode& iop, const std::function<OBJMResource(int16_t)>& idToOBJD);
};

class OBJM : public IffChunk
{
public:
    IFF_CHUNK_TYPE("ObjM")

    //house 00: 33 00 00 00
    //house 03: 3E 00 00 00
    //house 79: 45 00 00 00
    //completec:49 00 00 00
    uint32_t Version = 0;

    /// FreeSims kept the raw table: (objectID, OBJT TypeID) pairs, terminated by 0.
    std::vector<uint16_t> IDToOBJT;
    /// TS1(sims): FreeSO IDToOBJT (object id -> OBJT type id).
    std::map<uint16_t, uint16_t> IDToOBJTMap;

    class MappedObject {
    public:
        std::string Name;
        uint32_t GUID = 0;
        int32_t ObjectID = 0;
        int32_t Direction = 0;
        int32_t ParentID = 0;

        int32_t ContainerID = 0;
        int32_t ContainerSlot = 0;

        std::vector<int16_t> Data;

        int32_t ArryX = 0;
        int32_t ArryY = 0;
        int32_t ArryLevel = 0;

        /// TS1(sims): the decoded record (FreeSO OBJMInstance); null when only the heuristic worked.
        std::shared_ptr<OBJMInstance> Instance;

        std::string ToString() const
        {
            return Name.empty() ? "(unreferenced)" : Name;
        }
    };

    std::map<int32_t, MappedObject> ObjectData; //std::map: stable element addresses

    /// TS1(sims): true when Prepare decoded every per-object record.
    bool Decoded = false;
    std::string DecodeError;

    void Read(IffFile* iff, st::Reader& stream) override;

    /// TS1(sims): FreeSO OBJM.Prepare — decodes the per-object records with the house's OBJT. On failure the
    /// FreeSims heuristic ObjectData read by Read() is kept and DecodeError says why.
    bool Prepare(const std::function<OBJMResource(uint16_t typeID)>& typeIdToResource);

    std::vector<int32_t> SearchForObjectData(const std::vector<int16_t>& data);

private:
    struct CompressedData {
        int32_t Offset = 0;
        std::vector<uint8_t> Data;
    };
    std::vector<CompressedData> CompressedObjectInstances;
    void ReadHeuristic(const std::vector<uint8_t>& body);
};

}}}}} // namespace FSO::Files::Formats::IFF::Chunks
