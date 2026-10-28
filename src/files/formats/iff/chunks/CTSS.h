/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/. 
 */
// Translated from FreeSims sims.files/formats/iff/chunks/CTSS.cs
#pragma once
#include "files/formats/iff/chunks/STR.h"

namespace FSO { namespace Files { namespace Formats { namespace IFF { namespace Chunks {

/// <summary>
/// Catalog text strings; equivalent in format to STR#.
/// </summary>
class CTSS : public STR 
{
public:
    IFF_CHUNK_TYPE("CTSS")
};

}}}}} // namespace FSO::Files::Formats::IFF::Chunks
