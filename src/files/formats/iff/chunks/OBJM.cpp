/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
// Translated from FreeSims sims.files/formats/iff/chunks/OBJM.cs; per-object record decoding translated from FreeSO
// tso.files/Formats/IFF/Chunks/OBJM.cs (TS1(sims)).
#include "files/formats/iff/chunks/OBJM.h"
#include "files/formats/iff/IffFieldEncode.h"
#include "files/formats/iff/IffFile.h"
#include <stdexcept>
#include <algorithm>

namespace FSO { namespace Files { namespace Formats { namespace IFF { namespace Chunks {

// ---------------------------------------------------------------- TS1(sims): FreeSO record readers

void OBJMMultitile::Read(IffFieldEncode& iop) {
    OffsetX = iop.ReadInt32();
    OffsetY = iop.ReadInt32();
    OffsetLevel = iop.ReadInt32();
    GroupX = iop.ReadInt32();
    GroupY = iop.ReadInt32();
    GroupLevel = iop.ReadInt32();
    MultitileParentID = iop.ReadInt16();
}

void OBJMFootprint::Read(IffFieldEncode& iop) {
    MinY = iop.ReadInt32();
    MinX = iop.ReadInt32();
    MaxY = iop.ReadInt32();
    MaxX = iop.ReadInt32();
}

void OBJMMotiveDelta::Read(IffFieldEncode& iop) {
    Motive = iop.ReadInt32();
    TickDelta = iop.ReadFloat();
    StopAt = iop.ReadFloat();
}

void OBJMInteraction::Read(IffFieldEncode& iop) {
    int32_t unkZero = iop.ReadInt32();
    if (unkZero != 0) throw std::runtime_error("Expected zero at start of interaction...");
    UID = iop.ReadInt32();
    CallerID = iop.ReadInt16();
    TargetID = iop.ReadInt16();
    Icon = iop.ReadInt16();
    TTAIndex = iop.ReadInt32();
    for (int i = 0; i < 4; i++) Args[i] = iop.ReadInt16();
    Priority = iop.ReadInt32();
    ActionTreeID = iop.ReadInt16();
    Attenuation = iop.ReadFloat();
    Flags = iop.ReadInt32();
}

void OBJMAccessory::Read(IffFieldEncode& iop) {
    Name = iop.ReadString(false);
    Binding = iop.ReadString(false); // Assuming this is the binding - it's usually blank.
}

void OBJMObjectUse::Read(IffFieldEncode& iop) {
    TargetID = iop.ReadInt16();
    StackLength = iop.ReadInt32();
    Unknown2 = iop.ReadByte();
}

void OBJMPerson::Read(uint32_t version, IffFieldEncode& iop) {
    AnimEventCount = iop.ReadInt32();
    Engaged = iop.ReadInt32();

    Body = iop.ReadString(false); //b004mafit_01
    BodyTex = iop.ReadString(false); //BODY=b004mafitlgtrat
    Unk1 = iop.ReadString(false);
    Unk2 = iop.ReadString(false);
    LeftHand = iop.ReadString(false); //hmlo
    LeftHandTex = iop.ReadString(false); //HAND=huaolgt
    RightHand = iop.ReadString(false); //hmro
    RightHandTex = iop.ReadString(false); //HAND=huaolgt
    Head = iop.ReadString(false); //c013ma_pompa
    HeadTex = iop.ReadString(true); //HEAD-HEAD=c013malgt_pompa

    auto accessoryCount = iop.ReadInt32();
    if (accessoryCount < 0 || accessoryCount > 1024) throw std::runtime_error("OBJM person: bad accessory count");
    Accessories.resize((size_t)accessoryCount);
    for (auto& a : Accessories) a.Read(iop);

    // note: routing animation state is not saved
    Animation = iop.ReadString(false); //a2o-idle-neutral-lhips-look-1c;1;1000;240;1000;0;1;1
    CarryAnimation = iop.ReadString(false); //a2o-rarm-carry-loop;10;0;1000;1000;0;1;1
    BaseAnimation = iop.ReadString(true); //a2o-standing-loop;-10;1000;525;1000;1;1;1

    RoutingState = (OBJMRoutingState)iop.ReadInt32();
    for (int i = 0; i < 9; i++) FirstFloats[i] = iop.ReadFloat();
    for (int i = 0; i < 16; i++) MotiveDataOld[i] = iop.ReadFloat();
    for (int i = 0; i < 16; i++) MotiveData[i] = iop.ReadFloat();

    // Unsure of the exact point this field gets larger.
    // Unleashed prebuilds are 0x45 and have 0x50 fields.
    // Field 0x50 is from Superstar.
    // Related to change in NBRS.
    int dataCount = version > 0x45 ? 0x100 : 0x50;
    PersonData.resize((size_t)dataCount);
    for (auto& v : PersonData) v = iop.ReadInt16();

    RoutingFrameCount = iop.ReadInt32();

    ActiveInteraction.Read(iop);
    LastInteraction.Read(iop);

    auto interactionQueueCount = iop.ReadInt32();
    if (interactionQueueCount < 0 || interactionQueueCount > 1024) throw std::runtime_error("OBJM person: bad queue count");
    InteractionQueue.resize((size_t)interactionQueueCount);
    for (auto& q : InteractionQueue) q.Read(iop);

    int32_t useObjCount = iop.ReadInt32();
    if (useObjCount < 0 || useObjCount > 1024) throw std::runtime_error("OBJM person: bad object use count");
    ObjectUses.resize((size_t)useObjCount);
    for (auto& u : ObjectUses) u.Read(iop);

    int32_t motiveDeltas = iop.ReadInt32();
    if (motiveDeltas < 0 || motiveDeltas > 1024) throw std::runtime_error("OBJM person: bad motive delta count");
    MotiveDeltas.resize((size_t)motiveDeltas);
    for (auto& d : MotiveDeltas) d.Read(iop);
}

void OBJMStackFrame::Read(IffFieldEncode& iop) {
    StackObjectID = iop.ReadInt16();
    TreeID = iop.ReadInt16();
    NodeID = iop.ReadInt16();

    uint8_t locals = iop.ReadByte();
    uint8_t parameters = iop.ReadByte();
    Locals.resize(locals);
    Parameters.resize(parameters);
    for (int j = 0; j < parameters; j++) Parameters[j] = iop.ReadInt16();
    for (int j = 0; j < locals; j++) Locals[j] = iop.ReadInt16();

    PrimitiveState = iop.ReadInt32();
    CodeOwnerObjType = iop.ReadInt16(); // object type that owns the running code
}

void OBJMRelationshipEntry::Read(IffFieldEncode& iop) {
    IsPresent = iop.ReadInt32();
    if (IsPresent != 0) {
        TargetID = iop.ReadInt32();
        auto relToCount = iop.ReadInt32();
        if (relToCount < 0 || relToCount > 1024) throw std::runtime_error("OBJM: bad relationship value count");
        Values.resize((size_t)relToCount);
        for (auto& v : Values) v = iop.ReadInt32();
    } else {
        throw std::runtime_error("Unexpected IsPresent value: " + std::to_string(IsPresent));
    }
}

void OBJMInstance::Read(uint32_t version, IffFieldEncode& iop, const std::function<OBJMResource(int16_t)>& idToOBJD) {
    Footprint.Read(iop);

    X = iop.ReadInt32();
    Y = iop.ReadInt32();
    Level = iop.ReadInt32();
    if (Level != 1 && Level != 2) throw std::runtime_error("Unexpected level (should be 1/2): " + std::to_string(Level));

    UnknownData = iop.ReadInt16();

    int16_t attrCount = iop.ReadInt16();
    if (attrCount < 0) throw std::runtime_error("OBJM: bad attribute count");
    Attributes.resize((size_t)attrCount);
    for (auto& a : Attributes) a = iop.ReadInt16();

    TempRegisters.resize(TempCount);
    for (auto& t : TempRegisters) t = iop.ReadInt16();

    ObjectData.resize(ObjectDataCount);
    for (auto& d : ObjectData) d = iop.ReadInt16();

    ObjectDataExtra.resize(ExtraDataCount);
    for (auto& d : ObjectDataExtra) d = iop.ReadInt16();

    auto resources = idToOBJD(ObjectData[11]);
    OBJD = resources.OBJD;
    OBJT = resources.OBJT;
    if (OBJT == nullptr) throw std::runtime_error("OBJM: object id " + std::to_string(ObjectData[11]) + " has no OBJT entry");

    int32_t stackFrames = iop.ReadInt32();
    StackFlags = iop.ReadInt32();
    if (stackFrames < 0 || stackFrames > 256) throw std::runtime_error("OBJM: bad stack frame count");
    Stack.resize((size_t)stackFrames);
    for (auto& f : Stack) f.Read(iop);

    auto relFlag = iop.ReadInt32();
    if (relFlag < 0) {
        auto relCount = iop.ReadInt32();
        if (relCount < 0 || relCount > 4096) throw std::runtime_error("OBJM: bad relationship count");
        Relationships.resize((size_t)relCount);
        for (auto& r : Relationships) r.Read(iop);
    } else {
        throw std::runtime_error("Unknown relationship flag: " + std::to_string(relFlag));
    }

    auto slotsCount = iop.ReadInt16();
    if (slotsCount < 0) throw std::runtime_error("Unknown slots count: " + std::to_string(slotsCount));
    Slots.assign((size_t)std::max(0, slotsCount - 1), OBJMSlot());
    for (int i = 0; i < slotsCount; i++) {
        OBJMSlot slot;
        slot.Unknown = iop.ReadInt16();
        slot.ObjectID = iop.ReadInt16();
        if (i == 0) LinkedSlot = slot;
        else Slots[i - 1] = slot;
    }

    auto dsfCount = iop.ReadInt16();
    if (dsfCount < 0) throw std::runtime_error("OBJM: bad dynamic sprite flag count");
    DynamicSpriteFlags.resize((size_t)dsfCount);
    for (auto& f : DynamicSpriteFlags) f = iop.ReadInt16();

    // If we don't have the OBJD, assume the object is multitile if it has a blank name.
    HasMultitileData = (OBJD != nullptr) ? OBJD->IsMultiTile() : OBJT->Name.empty();
    if (HasMultitileData) MultitileData.Read(iop);

    if (OBJT->OBJDType == OBJDType::Portal) {
        int32_t portalDataCount = iop.ReadInt32();
        if (portalDataCount < 0 || portalDataCount > 1024) throw std::runtime_error("OBJM: bad portal data count");
        PortalData.resize((size_t)portalDataCount);
        for (auto& p : PortalData) p = iop.ReadFloat();
    }

    if (OBJT->OBJDType == OBJDType::Person) {
        HasPersonData = true;
        PersonData.Read(version, iop);
        UnknownInt = iop.ReadInt32();
    } else {
        HasPersonData = false;
        // "Placeholder" tends to go out of bounds here
        UnknownInt = iop.ReadInt32();
    }
}

// ---------------------------------------------------------------- OBJM

void OBJM::Read(IffFile* iff, st::Reader& stream)
{
    (void)iff;
    st::Reader& io = stream;
    io.ByteOrder(st::Endian::Little);
    {
        io.ReadUInt32(); //pad
        Version = io.ReadUInt32();

        //house 00: 33 00 00 00
        //house 03: 3E 00 00 00
        //house 79: 45 00 00 00
        //completec:49 00 00 00
        //corresponds to house version?

        auto MjbO = io.ReadUInt32();
        (void)MjbO; // FreeSO: must be 0x4f626a4d ("ObjM" magic)

        // Offsets are from here.
        int64_t offsetBase = io.Position();
        std::vector<uint8_t> body = io.ReadBytes((size_t)(io.Length() - offsetBase));
        ReadHeuristic(body);

        // TS1(sims): FreeSO — keep each object's compressed record for Prepare().
        CompressedObjectInstances.clear();
        IDToOBJTMap.clear();
        try {
            st::Reader b(body.data(), body.size(), st::Endian::Little);
            auto compressionCode = b.ReadByte();
            if (compressionCode != 1) throw std::runtime_error("Currently expects OBJM to be compressed");
            IffFieldEncode iop(&b);
            while (b.Position() < b.Length()) {
                auto id = iop.ReadUInt16();
                if (id == 0) break;
                auto type = iop.ReadUInt16();
                IDToOBJTMap[id] = type;
            }
            iop.Interrupt();
            size_t objCount = IDToOBJTMap.size();
            for (size_t i = 0; i < objCount; i++) {
                if (b.Position() + 4 > b.Length()) throw std::runtime_error("OBJM: object table truncated");
                auto skipOffset = b.ReadInt32();
                int64_t skipPosition = skipOffset; // relative to offsetBase == start of body
                CompressedData cd;
                cd.Offset = (int32_t)b.Position();
                if (skipPosition < b.Position() || skipPosition > b.Length()) throw std::runtime_error("OBJM: bad object record offset");
                cd.Data = b.ReadBytes((size_t)(skipPosition - b.Position()));
                CompressedObjectInstances.push_back(std::move(cd));
            }
            // The rest tends to be 0s, then ends with a3.
        } catch (std::exception& e) {
            CompressedObjectInstances.clear();
            DecodeError = e.what();
        }
    }
}

bool OBJM::Prepare(const std::function<OBJMResource(uint16_t typeID)>& typeIdToResource)
{
    if (CompressedObjectInstances.empty()) {
        if (DecodeError.empty()) DecodeError = "no object records";
        return false;
    }
    std::map<int32_t, MappedObject> decoded;
    auto idToResource = [&](int16_t objID) -> OBJMResource {
        auto it = IDToOBJTMap.find((uint16_t)objID);
        if (it == IDToOBJTMap.end()) throw std::runtime_error("OBJM: unknown object id " + std::to_string(objID));
        return typeIdToResource(it->second);
    };
    try {
        for (auto& instance : CompressedObjectInstances) {
            st::Reader io(instance.Data.data(), instance.Data.size(), st::Endian::Little);
            IffFieldEncode iop(&io, (instance.Offset & 1) == 1);
            auto obj = std::make_shared<OBJMInstance>();
            obj->Read(Version, iop, idToResource);

            MappedObject mapped;
            mapped.Instance = obj;
            mapped.ObjectID = obj->ObjectData[11];
            mapped.Direction = obj->ObjectData[1];
            mapped.ParentID = obj->ObjectData[26];
            mapped.ContainerID = obj->ObjectData[2];
            mapped.ContainerSlot = obj->ObjectData[3];
            mapped.Data = obj->ObjectData;
            if (obj->OBJT != nullptr) { mapped.Name = obj->OBJT->Name; mapped.GUID = obj->OBJT->GUID; }
            decoded[mapped.ObjectID] = mapped;
        }
    } catch (std::exception& e) {
        DecodeError = e.what();
        return false;
    }
    ObjectData = std::move(decoded);
    Decoded = true;
    return true;
}

// FreeSims heuristic (kept as the fallback when the records cannot be decoded).
void OBJM::ReadHeuristic(const std::vector<uint8_t>& body)
{
    st::Reader io(body.data(), body.size(), st::Endian::Little);
    auto compressionCode = io.ReadByte();
    if (compressionCode != 1) throw std::runtime_error("hey what!!");

    IffFieldEncode iop(&io);

    std::vector<uint16_t> table;
    while (io.HasMore())
    {
        auto value = iop.ReadUInt16();
        if (value == 0) break;
        table.push_back(value);
    }
    IDToOBJT = table;

    std::vector<int16_t> list;
    while (io.HasMore())
    {
        list.push_back(iop.ReadInt16());
    }

    auto offsets = SearchForObjectData(list);

    ObjectData.clear();
    int32_t lastOff = 0;
    for (auto off : offsets)
    {
        int32_t endOff = off + 72;
        int32_t size = endOff - lastOff;
        std::vector<int16_t> data;
        {
            int32_t start = std::max<int32_t>(0, lastOff);
            int32_t take = std::max<int32_t>(0, size);
            for (int32_t k = start; k < (int32_t)list.size() && k < start + take; k++) data.push_back(list[k]);
        }

        int32_t bas = size - 72;
        if (bas < 0 || bas + 26 >= (int32_t)data.size()) { lastOff = endOff; continue; }
        auto objID = data.at(bas+11); //object id
        auto dir = data.at(bas + 1);
        auto parent = data.at(bas + 26);
        auto containerid = data.at(bas + 2);
        auto containerslot = data.at(bas + 2);

        MappedObject mo;
        mo.ObjectID = objID; mo.Direction = dir; mo.Data = data; mo.ParentID = parent; mo.ContainerID = containerid; mo.ContainerSlot = containerslot;
        ObjectData[objID] = mo;

        lastOff = endOff;
    }
}

std::vector<int32_t> OBJM::SearchForObjectData(const std::vector<int16_t>& data)
{
    //we don't know exactly where the object data is in the format...
    //but we know objects should have a birth date, basically always 1997 or (1997-36) for npcs.
    //this should let us extract some important attributes like the structure of the data and object directions.

    std::vector<int32_t> offsets;
    for (int32_t i=0; i<(int32_t)data.size()-3; i++)
    {
        if ((data[i] == 1997 || data[i] == (1997-36)) && (data[i + 1] > 0 && data[i+1] < 13) && (data[i + 2] > 0 && data[i + 2] < 32)) {
            offsets.push_back(i - 45);
        }
    }

    return offsets;
}

}}}}} // namespace FSO::Files::Formats::IFF::Chunks
