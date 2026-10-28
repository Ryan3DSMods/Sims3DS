/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
// Translated from FreeSims sims.files/formats/iff/chunks/SPR2FrameEncoder.cs
#pragma once
#include "files/formats/iff/chunks/SPR2.h"
#include <functional>

namespace FSO { namespace Files { namespace Formats { namespace IFF { namespace Chunks {

class SPR2FrameEncoder
{
public:
    /// delegate Color[] QuantizerFunction(SPR2Frame frame, out byte[] bytes);
    typedef std::function<std::vector<Color>(SPR2Frame* frame, std::vector<uint8_t>& bytes)> QuantizerFunction;

    // STUB(3ds): set by the host application in C# (image quantizer); unset by default.
    static QuantizerFunction QuantizeFrame;

    static void WriteFrame(SPR2Frame* frame, st::Writer& output);

private:
    static uint8_t getCmd(uint8_t col, uint8_t a, uint8_t z);
};

}}}}} // namespace FSO::Files::Formats::IFF::Chunks
