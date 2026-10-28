/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/. 
 */
// Translated from FreeSims sims.files/formats/iff/chunks/TTAs.cs
#pragma once
#include "files/formats/iff/chunks/STR.h"

namespace FSO { namespace Files { namespace Formats { namespace IFF { namespace Chunks {

/// <summary>
/// Duplicate of STR chunk, instead used for pie menu strings.
/// </summary>
class TTAs : public STR
{
public:
    IFF_CHUNK_TYPE("TTAs")
    //no difference!
};

}}}}} // namespace FSO::Files::Formats::IFF::Chunks
