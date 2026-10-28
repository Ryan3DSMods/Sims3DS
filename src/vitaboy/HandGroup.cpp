/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
// FSO.Vitaboy.HandGroup (sims.common/rendering/vitaboy/HandGroup.cs)
#include "vitaboy/HandGroup.h"
#include "core/io.h"

namespace FSO { namespace Vitaboy {

void HandGroup::Read(const std::vector<uint8_t>& data) {
    st::Reader IOBuf(data, st::Endian::Big);
    m_Version = IOBuf.ReadUInt32();

    HandSet* sets[3] = { &LightSkin, &MediumSkin, &DarkSkin };
    for (auto set : sets) {
        Hand* hands[2] = { &set->RightHand, &set->LeftHand };
        for (auto hand : hands) {
            hand->Idle.FileID = IOBuf.ReadUInt32();
            hand->Idle.TypeID = IOBuf.ReadUInt32();
            hand->Fist.FileID = IOBuf.ReadUInt32();
            hand->Fist.TypeID = IOBuf.ReadUInt32();
            hand->Pointing.FileID = IOBuf.ReadUInt32();
            hand->Pointing.TypeID = IOBuf.ReadUInt32();
        }
    }
}

}}
