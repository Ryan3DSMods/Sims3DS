/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
// Translated from FreeSims sims.files/formats/iff/chunks/THMB.cs
#pragma once
#include "files/formats/iff/AbstractIffChunk.h"

namespace FSO { namespace Files { namespace Formats { namespace IFF { namespace Chunks {

class THMB : public IffChunk
{
public:
    IFF_CHUNK_TYPE("THMB")

    int32_t Width = 0;
    int32_t Height = 0;
    int32_t BaseYOff = 0;
    int32_t XOff = 0;
    int32_t AddYOff = 0; //accounts for difference between roofed and unroofed. relative to the base.

    void Read(IffFile* iff, st::Reader& stream) override;
};

}}}}} // namespace FSO::Files::Formats::IFF::Chunks
