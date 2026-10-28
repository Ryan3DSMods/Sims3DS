/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/. 
 */
// Translated from FreeSims sims.files/formats/iff/chunks/ARRY.cs
#include "files/formats/iff/chunks/ARRY.h"
#include "files/formats/iff/IffFile.h"

namespace FSO { namespace Files { namespace Formats { namespace IFF { namespace Chunks {

std::vector<uint8_t> ARRY::TransposeData() const
{
    int32_t stride = (int32_t)Type;
    std::vector<uint8_t> result(Data.size());
    for (size_t i = 0; i < Data.size(); i += stride)
    {
        int32_t divI = (int32_t)i / stride;
        int32_t x = divI % Width;
        int32_t y = divI / Width;
        int32_t targetIndex = y * stride + x * stride * Width;
        for (int32_t j = 0; j < stride; j++)
        {
            result[targetIndex++] = Data[i + j];
        }
    }
    return result;
}

void ARRY::Read(IffFile* iff, st::Reader& stream)
{
    (void)iff;
    st::Reader& io = stream;
    io.ByteOrder(st::Endian::Little);
    {
        auto zero = io.ReadInt32();
        (void)zero;
        Width = io.ReadInt32();
        Height = io.ReadInt32();
        Type = (ARRYType)io.ReadInt32();
        auto dataByteSize = ByteSize();
        Data = std::vector<uint8_t>((size_t)Width * Height * dataByteSize);
        auto unknown = io.ReadInt32();
        (void)unknown;

        int32_t currentPosition = 0;
        while (io.HasMore())
        {
            switch (Type)
            {
                default:
                {
                    //bit format:
                    //1000 yyyy yyxx xxxx

                    auto flre = io.ReadUInt16();
                    if (flre == 0) break;
                    bool rawFill = (flre & 0x8000) == 0;
                    if (rawFill)
                    {
                        for (int i=0; i<flre; i++)
                        {
                            if (!io.HasMore()) return;
                            Data[currentPosition++] = io.ReadByte();
                            if (currentPosition > (int32_t)Data.size()) return;
                        }
                        if ((flre & 1) == 1) io.ReadByte();
                        continue;
                    }

                    auto lastPosition = currentPosition;
                    currentPosition += flre & 0x7FFF;
                    if (currentPosition >= (int32_t)Data.size()) {
                        // FIX(sims): a run reaching the end of the grid still fills it with its pad value (FreeSims
                        // returned first, leaving zeros: the street rows of the buildable-area flags, ARRY 8 flag 32, read
                        // as buildable and the pool grids as 0 instead of 0xFF)
                        if (io.HasMore() || io.Position() < io.Length()) {
                            auto padEnd = io.ReadByte();
                            while (lastPosition < (int32_t)Data.size()) Data[lastPosition++] = padEnd;
                        }
                        return;
                    }
                    currentPosition %= (int32_t)Data.size(); //wrap to data size
                    if (currentPosition == 0) return;

                    auto pad = io.ReadByte();//ReadElement(io); //pad the previous entries with this data
                                            //if ((dataByteSize & 1) == 1) 
                    io.ReadByte(); //padded to 16 bits

                    while (lastPosition < currentPosition)
                    {
                        Data[lastPosition++] = pad;
                    }

                    if (!io.HasMore()) return;

                    auto size = io.ReadInt16();
                    if ((size & 0x8000) != 0)
                    {
                        io.Seek(st::SeekOrigin::Current, -2);
                        continue;
                    }
                    for (int i = 0; i < size; i++)
                    {
                        Data[currentPosition++] = io.ReadByte();//ReadElement(io);
                        currentPosition %= (int32_t)Data.size();
                    }

                    if ((size & 1) == 1) io.ReadByte(); //16-bit pad

                    currentPosition %= (int32_t)Data.size(); //12 bit wrap
                    break;
                }
            }

        }
    }
}

std::string ARRY::DebugPrint() const
{
    auto dataByteSize = ByteSize();
    std::string result;
    int index = 0;
    for (int y = 0; y < Height; y++)
    {
        for (int x = 0; x < Width; x++)
        {
            auto item = Data[index];
            auto str = std::to_string(item);
            while (str.size() < 3) str = " " + str;
            result += (str == "  0") ? "  ." : str;
            index += dataByteSize;
        }
        result += "\n";
    }
    return result;
}

}}}}} // namespace FSO::Files::Formats::IFF::Chunks
