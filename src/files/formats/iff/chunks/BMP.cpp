/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/. 
 */
// Translated from FreeSims sims.files/formats/iff/chunks/BMP.cs
#include "files/formats/iff/chunks/BMP.h"
#include "files/formats/iff/IffFile.h"

namespace FSO { namespace Files { namespace Formats { namespace IFF { namespace Chunks {

void BMP::Read(IffFile* iff, st::Reader& stream)
{
    (void)iff;
    data = stream.ReadBytes((size_t)stream.Length());
}

}}}}} // namespace FSO::Files::Formats::IFF::Chunks
