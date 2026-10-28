/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
// Translated from FreeSims sims.files/formats/iff/chunks/SPR2FrameEncoder.cs
#include "files/formats/iff/chunks/SPR2FrameEncoder.h"
#include <cmath>

namespace FSO { namespace Files { namespace Formats { namespace IFF { namespace Chunks {

SPR2FrameEncoder::QuantizerFunction SPR2FrameEncoder::QuantizeFrame;

void SPR2FrameEncoder::WriteFrame(SPR2Frame* frame, st::Writer& output)
{
    auto& bytes = frame->PalData;
    auto& zs = frame->ZBufferData;

    int32_t index = 0;
    int32_t blankLines = 0;
    for (int32_t y=0; y<frame->Height; y++)
    {
        uint8_t lastCmd = 0;
        std::vector<uint8_t> dataBuf;
        int32_t rlecount = 0;
        bool anySolid = false;

        st::Writer scanOut(st::Endian::Little);

        for (int32_t x=0; x<frame->Width; x++)
        {
            uint8_t plt = bytes[index];
            uint8_t a = frame->AlphaAt((size_t)index); // FIX(sims): palettised frames (see SPR2.h)
            uint8_t z = zs[index];

            auto cmd = getCmd(plt, a, z);

            if (x == 0 || cmd != lastCmd)
            {
                if (x != 0)
                {
                    //write a command to write the last sequence of pixels
                    scanOut.WriteUInt16((uint16_t)(((int32_t)lastCmd<<13)|rlecount));
                    if ((dataBuf.size() % 2) != 0) dataBuf.push_back(0);
                    scanOut.WriteBytes(dataBuf);
                    dataBuf.clear();
                }

                lastCmd = cmd;
                rlecount = 0;
            }

            switch (cmd)
            {
                case 0x1:
                case 0x2:
                    dataBuf.push_back(z);
                    dataBuf.push_back(plt);
                    if (cmd == 0x2) dataBuf.push_back((uint8_t)std::ceil(a/ 8.2258064516129032258064516129032));
                    anySolid = true;
                    break;
                case 0x6:
                    dataBuf.push_back(plt);
                    anySolid = true;
                    break;
                default:
                    break;
            }
            rlecount++;

            index++;
        }

        if (anySolid)
        {
            //write a command to write the last sequence of pixels
            scanOut.WriteUInt16((uint16_t)(((int32_t)lastCmd << 13) | rlecount));
            if ((dataBuf.size() % 2) != 0) dataBuf.push_back(0);
            scanOut.WriteBytes(dataBuf);
            dataBuf.clear();
        }

        auto& scanData = scanOut.ToArray();

        if (scanData.size() == 0)
            blankLines++; //line is transparent
        else 
        {
            if (blankLines > 0)
            {
                //add transparent lines before our new command
                output.WriteUInt16((uint16_t)((0x4<<13) | blankLines));
                blankLines = 0;
            }
            output.WriteUInt16((uint16_t)(scanData.size()+2));
            output.WriteBytes(scanData);
        }

    }

    if (blankLines > 0)
    {
        //add transparent lines before our new command
        output.WriteUInt16((uint16_t)((0x4 << 13) | blankLines));
        blankLines = 0;
    }
    output.WriteUInt16((uint16_t)(0x5<<13));

    return;
}

uint8_t SPR2FrameEncoder::getCmd(uint8_t col, uint8_t a, uint8_t z)
{
    (void)col;
    if (a == 0) return 0x03; //transparent fill
    else if (a < 255) return 0x02; // col,a,z
    else if (z > 0) return 0x01; // col,z
    else return 0x06; // col
}

}}}}} // namespace FSO::Files::Formats::IFF::Chunks
