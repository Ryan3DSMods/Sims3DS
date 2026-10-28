/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
// Translated from FreeSims sims.files/formats/iff/ChunkRuntimeInfo.cs
#pragma once

namespace FSO { namespace Files { namespace Formats { namespace IFF {

enum class ChunkRuntimeState
{
    Normal,
    Patched, //unmodified, but still save when outputting PIFF
    Modified, //modified. save when outputting PIFF
    Delete //this chunk should not be saved, or should be saved as a deletion.
};

}}}} // namespace FSO::Files::Formats::IFF
