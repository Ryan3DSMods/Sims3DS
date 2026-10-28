/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
// Translated from FreeSims sims.files/formats/iff/chunks/TPRP.cs
#pragma once
#include "files/formats/iff/AbstractIffChunk.h"

namespace FSO { namespace Files { namespace Formats { namespace IFF { namespace Chunks {

/// <summary>
/// Labels for BHAV local variables and parameters.
/// </summary>
class TPRP : public IffChunk
{
public:
    IFF_CHUNK_TYPE("TPRP")

    std::vector<std::string> ParamNames;
    std::vector<std::string> LocalNames;

    /// <summary>
    /// Reads a TPRP from a stream.
    /// </summary>
    void Read(IffFile* iff, st::Reader& stream) override;
};

}}}}} // namespace FSO::Files::Formats::IFF::Chunks
