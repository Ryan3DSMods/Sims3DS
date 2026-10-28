/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/. 
 */
// Translated from FreeSims sims.files/formats/iff/chunks/FAMs.cs
#pragma once
#include "files/formats/iff/chunks/STR.h"

namespace FSO { namespace Files { namespace Formats { namespace IFF { namespace Chunks {

class FAMs : public STR
{
public:
    IFF_CHUNK_TYPE("FAMs")
};

}}}}} // namespace FSO::Files::Formats::IFF::Chunks
