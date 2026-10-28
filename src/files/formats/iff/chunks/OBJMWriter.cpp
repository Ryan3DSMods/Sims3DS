/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
// TS1(sims): OBJM writer, the mirror of FreeSO's OBJM reader (OBJM.cpp). See OBJMWriter.h.
#include "files/formats/iff/chunks/OBJMWriter.h"
#include "files/formats/iff/IffFieldEncode.h"
#include "core/io.h"
#include <stdexcept>

namespace FSO { namespace Files { namespace Formats { namespace IFF { namespace Chunks {

void WriteOBJMMultitile(const OBJMMultitile& m, IffFieldEncodeWriter& iop) {
    iop.WriteInt32(m.OffsetX);
    iop.WriteInt32(m.OffsetY);
    iop.WriteInt32(m.OffsetLevel);
    iop.WriteInt32(m.GroupX);
    iop.WriteInt32(m.GroupY);
    iop.WriteInt32(m.GroupLevel);
    iop.WriteInt16(m.MultitileParentID);
}

void WriteOBJMFootprint(const OBJMFootprint& f, IffFieldEncodeWriter& iop) {
    iop.WriteInt32(f.MinY);
    iop.WriteInt32(f.MinX);
    iop.WriteInt32(f.MaxY);
    iop.WriteInt32(f.MaxX);
}

void WriteOBJMMotiveDelta(const OBJMMotiveDelta& d, IffFieldEncodeWriter& iop) {
    iop.WriteInt32(d.Motive);
    iop.WriteFloat(d.TickDelta);
    iop.WriteFloat(d.StopAt);
}

void WriteOBJMInteraction(const OBJMInteraction& i, IffFieldEncodeWriter& iop) {
    iop.WriteInt32(0); // unkZero
    iop.WriteInt32(i.UID);
    iop.WriteInt16(i.CallerID);
    iop.WriteInt16(i.TargetID);
    iop.WriteInt16(i.Icon);
    iop.WriteInt32(i.TTAIndex);
    for (int k = 0; k < 4; k++) iop.WriteInt16(i.Args[k]);
    iop.WriteInt32(i.Priority);
    iop.WriteInt16(i.ActionTreeID);
    iop.WriteFloat(i.Attenuation);
    iop.WriteInt32(i.Flags);
}

void WriteOBJMAccessory(const OBJMAccessory& a, IffFieldEncodeWriter& iop) {
    iop.WriteString(a.Name, false);
    iop.WriteString(a.Binding, false);
}

void WriteOBJMObjectUse(const OBJMObjectUse& u, IffFieldEncodeWriter& iop) {
    iop.WriteInt16(u.TargetID);
    iop.WriteInt32(u.StackLength);
    iop.WriteByte(u.Unknown2);
}

void WriteOBJMPerson(const OBJMPerson& p, uint32_t version, IffFieldEncodeWriter& iop) {
    iop.WriteInt32(p.AnimEventCount);
    iop.WriteInt32(p.Engaged);

    iop.WriteString(p.Body, false);
    iop.WriteString(p.BodyTex, false);
    iop.WriteString(p.Unk1, false);
    iop.WriteString(p.Unk2, false);
    iop.WriteString(p.LeftHand, false);
    iop.WriteString(p.LeftHandTex, false);
    iop.WriteString(p.RightHand, false);
    iop.WriteString(p.RightHandTex, false);
    iop.WriteString(p.Head, false);
    iop.WriteString(p.HeadTex, true);

    iop.WriteInt32((int32_t)p.Accessories.size());
    for (auto& a : p.Accessories) WriteOBJMAccessory(a, iop);

    iop.WriteString(p.Animation, false);
    iop.WriteString(p.CarryAnimation, false);
    iop.WriteString(p.BaseAnimation, true);

    iop.WriteInt32((int32_t)p.RoutingState);
    for (int i = 0; i < 9; i++) iop.WriteFloat(p.FirstFloats[i]);
    for (int i = 0; i < 16; i++) iop.WriteFloat(p.MotiveDataOld[i]);
    for (int i = 0; i < 16; i++) iop.WriteFloat(p.MotiveData[i]);

    size_t dataCount = version > 0x45 ? 0x100 : 0x50;
    for (size_t i = 0; i < dataCount; i++) iop.WriteInt16(i < p.PersonData.size() ? p.PersonData[i] : (int16_t)0);

    iop.WriteInt32(p.RoutingFrameCount);

    WriteOBJMInteraction(p.ActiveInteraction, iop);
    WriteOBJMInteraction(p.LastInteraction, iop);

    iop.WriteInt32((int32_t)p.InteractionQueue.size());
    for (auto& q : p.InteractionQueue) WriteOBJMInteraction(q, iop);

    iop.WriteInt32((int32_t)p.ObjectUses.size());
    for (auto& u : p.ObjectUses) WriteOBJMObjectUse(u, iop);

    iop.WriteInt32((int32_t)p.MotiveDeltas.size());
    for (auto& d : p.MotiveDeltas) WriteOBJMMotiveDelta(d, iop);
}

void WriteOBJMStackFrame(const OBJMStackFrame& f, IffFieldEncodeWriter& iop) {
    iop.WriteInt16(f.StackObjectID);
    iop.WriteInt16(f.TreeID);
    iop.WriteInt16(f.NodeID);
    iop.WriteByte((uint8_t)f.Locals.size());
    iop.WriteByte((uint8_t)f.Parameters.size());
    for (auto v : f.Parameters) iop.WriteInt16(v);
    for (auto v : f.Locals) iop.WriteInt16(v);
    iop.WriteInt32(f.PrimitiveState);
    iop.WriteInt16(f.CodeOwnerObjType);
}

void WriteOBJMRelationshipEntry(const OBJMRelationshipEntry& r, IffFieldEncodeWriter& iop) {
    iop.WriteInt32(r.IsPresent == 0 ? 1 : r.IsPresent); // the reader only accepts present entries
    iop.WriteInt32(r.TargetID);
    iop.WriteInt32((int32_t)r.Values.size());
    for (auto v : r.Values) iop.WriteInt32(v);
}

void WriteOBJMInstance(const OBJMInstance& inst, uint32_t version, IffFieldEncodeWriter& iop, bool hasLinkedSlot,
                       const std::string& unhandledBits) {
    WriteOBJMFootprint(inst.Footprint, iop);
    iop.WriteInt32(inst.X);
    iop.WriteInt32(inst.Y);
    iop.WriteInt32(inst.Level);
    iop.WriteInt16(inst.UnknownData);

    iop.WriteInt16((int16_t)inst.Attributes.size());
    for (auto a : inst.Attributes) iop.WriteInt16(a);
    for (int i = 0; i < OBJMInstance::TempCount; i++) iop.WriteInt16(i < (int)inst.TempRegisters.size() ? inst.TempRegisters[i] : (int16_t)0);
    for (int i = 0; i < OBJMInstance::ObjectDataCount; i++) iop.WriteInt16(i < (int)inst.ObjectData.size() ? inst.ObjectData[i] : (int16_t)0);
    for (int i = 0; i < OBJMInstance::ExtraDataCount; i++) iop.WriteInt16(i < (int)inst.ObjectDataExtra.size() ? inst.ObjectDataExtra[i] : (int16_t)0);

    iop.WriteInt32((int32_t)inst.Stack.size());
    iop.WriteInt32(inst.StackFlags);
    for (auto& f : inst.Stack) WriteOBJMStackFrame(f, iop);

    iop.WriteInt32(-1); // relFlag (< 0: relationship list follows; the only form the reader accepts)
    iop.WriteInt32((int32_t)inst.Relationships.size());
    for (auto& r : inst.Relationships) WriteOBJMRelationshipEntry(r, iop);

    if (!hasLinkedSlot && inst.Slots.empty()) {
        iop.WriteInt16(0);
    } else {
        iop.WriteInt16((int16_t)(inst.Slots.size() + 1));
        iop.WriteInt16(inst.LinkedSlot.Unknown);
        iop.WriteInt16(inst.LinkedSlot.ObjectID);
        for (auto& s : inst.Slots) { iop.WriteInt16(s.Unknown); iop.WriteInt16(s.ObjectID); }
    }

    iop.WriteInt16((int16_t)inst.DynamicSpriteFlags.size());
    for (auto f : inst.DynamicSpriteFlags) iop.WriteInt16(f);

    if (inst.HasMultitileData) WriteOBJMMultitile(inst.MultitileData, iop);

    if (inst.OBJT != nullptr && inst.OBJT->OBJDType == OBJDType::Portal) {
        iop.WriteInt32((int32_t)inst.PortalData.size());
        for (auto p : inst.PortalData) iop.WriteFloat(p);
    }

    if (inst.HasPersonData) WriteOBJMPerson(inst.PersonData, version, iop);
    iop.WriteInt32(inst.UnknownInt);

    iop.WriteBitString(unhandledBits);
}

bool OBJMRawChunk::Parse(const std::vector<uint8_t>& chunkData, std::string& error)
{
    try {
        st::Reader io(chunkData.data(), chunkData.size(), st::Endian::Little);
        Pad = io.ReadUInt32();
        Version = io.ReadUInt32();
        Magic = io.ReadUInt32();
        if (Magic != 0x4f626a4d) throw std::runtime_error("Invalid OBJM magic number");
        int64_t offsetBase = io.Position();
        std::vector<uint8_t> body(chunkData.begin() + offsetBase, chunkData.end());
        st::Reader b(body.data(), body.size(), st::Endian::Little);
        Compression = b.ReadByte();
        if (Compression != 1) throw std::runtime_error("OBJM is not compressed");
        IffFieldEncode iop(&b);
        Table.clear();
        while (b.Position() < b.Length()) {
            auto id = iop.ReadUInt16();
            if (id == 0) break;
            auto type = iop.ReadUInt16();
            Table.push_back(std::make_pair(id, type));
        }
        iop.Interrupt();
        Records.clear();
        for (size_t i = 0; i < Table.size(); i++) {
            if (b.Position() + 4 > b.Length()) throw std::runtime_error("OBJM: object table truncated");
            int32_t skip = b.ReadInt32();
            Record r;
            r.Offset = (int32_t)b.Position();
            if (skip < b.Position() || skip > b.Length()) throw std::runtime_error("OBJM: bad object record offset");
            r.Data = b.ReadBytes((size_t)(skip - b.Position()));
            Records.push_back(std::move(r));
        }
        Tail = b.ReadBytes((size_t)(b.Length() - b.Position()));
        return true;
    } catch (std::exception& e) {
        error = e.what();
        return false;
    }
}

std::vector<uint8_t> WriteOBJMChunk(uint32_t pad, uint32_t version, const std::vector<OBJMRecordOut>& records,
                                    const std::vector<uint8_t>& tail)
{
    std::vector<uint8_t> body;
    {
        IffFieldEncodeWriter iop(0);
        iop.Data.push_back(1); // compression code
        for (auto& r : records) {
            iop.WriteUInt16(r.ObjectID);
            iop.WriteUInt16(r.TypeID);
        }
        iop.WriteUInt16(0);
        iop.Interrupt();
        body = std::move(iop.Data);
    }
    auto put32 = [&](int32_t v) {
        for (int k = 0; k < 4; k++) body.push_back((uint8_t)((uint32_t)v >> (8 * k)));
    };
    for (auto& r : records) {
        size_t skipPos = body.size();
        put32(0);
        int64_t start = (int64_t)body.size();
        if (r.UseRaw && ((start & 1) == (r.RawOffsetParity & 1))) {
            body.insert(body.end(), r.Raw.begin(), r.Raw.end());
        } else {
            IffFieldEncodeWriter iop(start);
            WriteOBJMInstance(r.Instance, version, iop, r.HasLinkedSlot, r.UnhandledBits);
            body.insert(body.end(), iop.Data.begin(), iop.Data.end());
        }
        int32_t end = (int32_t)body.size();
        for (int k = 0; k < 4; k++) body[skipPos + k] = (uint8_t)((uint32_t)end >> (8 * k));
    }
    body.insert(body.end(), tail.begin(), tail.end());

    std::vector<uint8_t> out;
    st::Writer w(st::Endian::Little);
    w.WriteUInt32(pad);
    w.WriteUInt32(version);
    w.WriteUInt32(0x4f626a4d);
    w.WriteBytes(body);
    return w.TakeArray();
}

}}}}} // namespace FSO::Files::Formats::IFF::Chunks
