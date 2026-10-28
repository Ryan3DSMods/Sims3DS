/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/. 
 */
// Translated from FreeSims sims.files/formats/iff/chunks/SLOT.cs
#include "files/formats/iff/chunks/SLOT.h"
#include "files/formats/iff/IffFile.h"

namespace FSO { namespace Files { namespace Formats { namespace IFF { namespace Chunks {

const float SLOT::HeightOffsets[9] = {
    //NOTE: 1 indexed! to get offset for a height, lookup (SLOT.Height-1)
    0, //floor
    2.5f, //low table
    4, //table
    4, //counter
    0, //non-standard (appears to use offset height)
    0, //in hand (unused probably. we handle avatar hands as a special case.)
    7, //sitting (used for chairs)
    4, //end table
    0 //TODO: unknown
};

void SLOT::Read(IffFile* iff, st::Reader& stream)
{
    (void)iff;
    st::Reader& io = stream;
    io.ByteOrder(st::Endian::Little);
    {
        auto zero = io.ReadUInt32();
        (void)zero;
        auto version = io.ReadUInt32();
        auto slotMagic = io.ReadBytes(4);
        (void)slotMagic;
        auto numSlots = io.ReadUInt32();

        Slots.clear();

        /** The span for version 4 is 34. 
         * The span for version 6 is 54. 
         * The span for version 7 is 58.  
         * The span for version 8 is 62. 
         * The span for version 9 is 66. 
         * The span for version 10 is 70.  **/
        for (uint32_t i = 0; i < numSlots; i++){
            SLOTItem item;
            item.Type = io.ReadUInt16();
            float ox = io.ReadFloat();
            float oy = io.ReadFloat();
            float oz = io.ReadFloat();
            item.Offset = Vector3(ox, oy, oz);

            auto standing = io.ReadInt32();
            auto sitting = io.ReadInt32();
            auto ground = io.ReadInt32();
            auto rsflags = io.ReadInt32();
            auto snaptargetslot = io.ReadInt32();

            //bonuses (0 means never)
            item.Standing = standing; //score bonus for standing destinations
            item.Sitting = sitting; //score bonus for sitting destinations
            item.Ground = ground; //score bonus for sitting on ground 

            item.Rsflags = (SLOTFlags)rsflags;
            item.SnapTargetSlot = snaptargetslot;

            if (version >= 6)
            {
                auto minproximity = io.ReadInt32();
                auto maxproximity = io.ReadInt32();
                auto optimalproximity = io.ReadInt32();
                auto i9 = io.ReadInt32();
                auto i10 = io.ReadInt32();
                (void)i9; (void)i10;

                item.MinProximity = minproximity;
                item.MaxProximity = maxproximity;
                item.OptimalProximity = optimalproximity;
            }

            if (version <= 9) {
                item.MinProximity *= 16;
                item.MaxProximity *= 16;
                item.OptimalProximity *= 16;
            }

            if (version >= 7) item.Gradient = io.ReadFloat();

            if (version >= 8) item.Height = io.ReadInt32();

            //the below cases are just here for breakpoint purposes
            //TODO: find use cases?
            if (item.Height == 9) item.Height = 9;

            if (item.Height == 0) item.Height = 5; //use offset height, nonstandard.

            if (version >= 9) item.Facing = (SLOTFacing)io.ReadInt32();

            if (version >= 10) item.Resolution = io.ReadInt32();

            Slots[item.Type].push_back(item);
        }
    }
}

}}}}} // namespace FSO::Files::Formats::IFF::Chunks
