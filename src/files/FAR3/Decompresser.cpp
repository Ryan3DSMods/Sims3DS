/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/. 
 */
// Translated from FreeSims sims.files/FAR3/Decompresser.cs
#include "files/FAR3/Decompresser.h"
#include <unordered_map>
#include <stdexcept>

namespace FSO { namespace Files { namespace FAR3 {

void Decompresser::ArrayCopy2(const std::vector<uint8_t>& Src, int32_t SrcPos, std::vector<uint8_t>& Dest, int32_t DestPos, int64_t Length)
{
    if ((int64_t)Dest.size() < DestPos + Length)
    {
        Dest.resize((size_t)(DestPos + Length));
    }

    for (int64_t i = 0; i < Length/* - 1*/; i++)
        Dest[DestPos + i] = Src.at(SrcPos + i);
}

void Decompresser::OffsetCopy(std::vector<uint8_t>& array, int32_t srcPos, int32_t destPos, int64_t length)
{
    srcPos = destPos - srcPos;

    if ((int64_t)array.size() < destPos + length)
    {
        array.resize((size_t)(destPos + length));
    }

    for (int64_t i = 0; i < length /*- 1*/; i++)
    {
        array[destPos + i] = array.at(srcPos + i);
    }
}

std::vector<uint8_t> Decompresser::Compress(const std::vector<uint8_t>& Data)
{
    // if data is big enough for compress
    if (Data.size() > 6)
    {
        // some Compression Data
        const int32_t MAX_OFFSET = 0x20000;
        const int32_t MAX_COPY_COUNT = 0x404;
        // used to finetune the lookup (small values increase the
        // compression for Big Files)
        const int32_t QFS_MAXITER = 0x80;

        // contains the latest offset for a combination of two
        // characters
        std::unordered_map<int32_t, std::vector<int32_t>> cmpmap2;

        // will contain the compressed data (maximal size =
        // uncompressedSize+MAX_COPY_COUNT)
        std::vector<uint8_t> cData(Data.size() + MAX_COPY_COUNT);

        // init some vars
        int32_t writeIndex = 9; // leave 9 bytes for the header
        int32_t lastReadIndex = 0;
        std::vector<int32_t>* indexList = nullptr;
        int32_t copyOffset = 0;
        int32_t copyCount = 0;
        int32_t index = -1;
        bool end = false;
        const int32_t DataLength = (int32_t)Data.size();

        // begin main compression loop
        while (index < DataLength - 3)
        {
            // get all Compression Candidates (list of offsets for all
            // occurances of the current 3 bytes)
            do 
            {
                index++;
                if (index >= DataLength - 2)
                {
                    end = true;
                    break;
                }
                int32_t mapindex = Data[index] + (Data[index + 1] << 8)
                        + (Data[index + 2] << 16);

                indexList = &cmpmap2[mapindex];
                indexList->push_back(index);
            } while (index < lastReadIndex);
            if (end)
                break;

            // find the longest repeating byte sequence in the index
            // List (for offset copy)
            int32_t offsetCopyCount = 0;
            int32_t loopcount = 1;
            while ((loopcount < (int32_t)indexList->size()) && (loopcount < QFS_MAXITER))
            {
                int32_t foundindex = (*indexList)[(indexList->size() - 1) - loopcount];
                if ((index - foundindex) >= MAX_OFFSET)
                {
                    break;
                }

                loopcount++;
                copyCount = 3;

                while ((DataLength > index + copyCount)&& (Data[index + copyCount] == Data[foundindex + copyCount]) && (copyCount < MAX_COPY_COUNT))
                {
                    copyCount++;
                }

                if (copyCount > offsetCopyCount)
                {
                    offsetCopyCount = copyCount;
                    copyOffset = index - foundindex;
                }
            }

            // check if we can compress this
            // In FSH Tool stand additionally this:
            if (offsetCopyCount > DataLength - index)
            {
                offsetCopyCount = index - DataLength;
            }
            if (offsetCopyCount <= 2)
            {
                offsetCopyCount = 0;
            } 
            else if ((offsetCopyCount == 3) && (copyOffset > 0x400)) 
            { // 1024
                offsetCopyCount = 0;
            } 
            else if ((offsetCopyCount == 4) && (copyOffset > 0x4000)) 
            { // 16384
                offsetCopyCount = 0;
            }

            // this is offset-compressable? so do the compression
            if (offsetCopyCount > 0)
            {
                // plaincopy

                // In FSH Tool stand this (A):
                while (index - lastReadIndex >= 4)
                {
                    copyCount = (index - lastReadIndex) / 4 - 1;
                    if (copyCount > 0x1B)
                    {
                        copyCount = 0x1B;
                    }
                    cData[writeIndex++] = (uint8_t)(0xE0 + copyCount);
                    copyCount = 4 * copyCount + 4;

                    ArrayCopy2(Data, lastReadIndex, cData, writeIndex, copyCount);
                    lastReadIndex += copyCount;
                    writeIndex += copyCount;
                }

                // offsetcopy
                copyCount = index - lastReadIndex;
                copyOffset--;
                if ((offsetCopyCount <= 0x0A) && (copyOffset < 0x400))
                {
                    cData[writeIndex++] = (uint8_t) (((copyOffset >> 8) << 5)
                            + ((offsetCopyCount - 3) << 2) + copyCount);
                    cData[writeIndex++] = (uint8_t)(copyOffset & 0xff);
                } 
                else if ((offsetCopyCount <= 0x43) && (copyOffset < 0x4000))
                {
                    cData[writeIndex++] = (uint8_t)(0x80 + (offsetCopyCount - 4));
                    cData[writeIndex++] = (uint8_t) ((copyCount << 6) + (copyOffset >> 8));
                    cData[writeIndex++] = (uint8_t) (copyOffset & 0xff);
                } 
                else if ((offsetCopyCount <= MAX_COPY_COUNT) && (copyOffset < MAX_OFFSET))
                {
                    cData[writeIndex++] = (uint8_t)(0xc0
                            + ((copyOffset >> 16) << 4)
                            + (((offsetCopyCount - 5) >> 8) << 2) + copyCount);
                    cData[writeIndex++] = (uint8_t)((copyOffset >> 8) & 0xff);
                    cData[writeIndex++] = (uint8_t)(copyOffset & 0xff);
                    cData[writeIndex++] = (uint8_t)((offsetCopyCount - 5) & 0xff);
                }

                // do the offset copy
                ArrayCopy2(Data, lastReadIndex, cData, writeIndex, copyCount);
                writeIndex += copyCount;
                lastReadIndex += copyCount;
                lastReadIndex += offsetCopyCount;
            }
        }

        // add the End Record
        index = DataLength;
        // in FSH Tool stand the same as above (A)
        while (index - lastReadIndex >= 4)
        {
            copyCount = (index - lastReadIndex) / 4 - 1;
            
            if (copyCount > 0x1B)
                copyCount = 0x1B;

            cData[writeIndex++] = (uint8_t)(0xE0 + copyCount);
            copyCount = 4 * copyCount + 4;

            ArrayCopy2(Data, lastReadIndex, cData, writeIndex, copyCount);
            lastReadIndex += copyCount;
            writeIndex += copyCount;
        }

        copyCount = index - lastReadIndex;
        cData[writeIndex++] = (uint8_t) (0xfc + copyCount);
        ArrayCopy2(Data, lastReadIndex, cData, writeIndex, copyCount);
        writeIndex += copyCount;
        lastReadIndex += copyCount;

        std::vector<uint8_t> DataStream;
        auto WriteU32 = [&](uint32_t v) { DataStream.push_back((uint8_t)v); DataStream.push_back((uint8_t)(v >> 8)); DataStream.push_back((uint8_t)(v >> 16)); DataStream.push_back((uint8_t)(v >> 24)); };
        auto WriteU16 = [&](uint16_t v) { DataStream.push_back((uint8_t)v); DataStream.push_back((uint8_t)(v >> 8)); };

        // write the header for the compressed data
        // set the compressed size
        WriteU32((uint32_t)writeIndex);
        m_CompressedSize = writeIndex;
        // set the MAGICNUMBER
        WriteU16((uint16_t)0xFB10);
        // set the decompressed size
        uint32_t len = (uint32_t)DataLength;
        uint8_t revData[4] = { (uint8_t)len, (uint8_t)(len >> 8), (uint8_t)(len >> 16), (uint8_t)(len >> 24) };
        WriteU32((uint32_t)((revData[2] << 16) | (revData[1] << 8) | revData[0]));
        DataStream.insert(DataStream.end(), cData.begin(), cData.end());

        m_DecompressedSize = DataLength;

        return DataStream;
    }

    return Data;
}

std::vector<uint8_t> Decompresser::Decompress(const std::vector<uint8_t>& Data)
{
    if (Data.size() > 6)
    {
        std::vector<uint8_t> DecompressedData((size_t)m_DecompressedSize);
        int32_t DataPos = 0;

        int32_t Pos = 0;
        int64_t Control1 = 0;
        const int32_t DataLength = (int32_t)Data.size();

        while (Control1 != 0xFC && Pos < DataLength)
        {
            Control1 = Data[Pos];
            Pos++;

            if (Pos == DataLength)
                break;

            if (Control1 >= 0 && Control1 <= 127)
            {
                // 0x00 - 0x7F
                int64_t control2 = Data[Pos];
                Pos++;
                int64_t numberOfPlainText = (Control1 & 0x03);
                ArrayCopy2(Data, Pos, DecompressedData, DataPos, numberOfPlainText);
                DataPos += (int32_t)numberOfPlainText;
                Pos += (int32_t)numberOfPlainText;

                if (DataPos == (int32_t)DecompressedData.size())
                    break;

                int32_t offset = (int32_t)(((Control1 & 0x60) << 3) + (control2) + 1);
                int64_t numberToCopyFromOffset = ((Control1 & 0x1C) >> 2) + 3;
                OffsetCopy(DecompressedData, offset, DataPos, numberToCopyFromOffset);
                DataPos += (int32_t)numberToCopyFromOffset;

                if (DataPos == (int32_t)DecompressedData.size())
                    break;
            }
            else if ((Control1 >= 128 && Control1 <= 191))
            {
                // 0x80 - 0xBF
                int64_t control2 = Data[Pos];
                Pos++;
                int64_t control3 = Data[Pos];
                Pos++;

                int64_t numberOfPlainText = (control2 >> 6) & 0x03;
                ArrayCopy2(Data, Pos, DecompressedData, DataPos, numberOfPlainText);
                DataPos += (int32_t)numberOfPlainText;
                Pos += (int32_t)numberOfPlainText;

                if (DataPos == (int32_t)DecompressedData.size())
                    break;

                int32_t offset = (int32_t)(((control2 & 0x3F) << 8) + (control3) + 1);
                int64_t numberToCopyFromOffset = (Control1 & 0x3F) + 4;
                OffsetCopy(DecompressedData, offset, DataPos, numberToCopyFromOffset);
                DataPos += (int32_t)numberToCopyFromOffset;

                if (DataPos == (int32_t)DecompressedData.size())
                    break;
            }
            else if (Control1 >= 192 && Control1 <= 223)
            {
                // 0xC0 - 0xDF
                int64_t numberOfPlainText = (Control1 & 0x03);
                int64_t control2 = Data[Pos];
                Pos++;
                int64_t control3 = Data[Pos];
                Pos++;
                int64_t control4 = Data[Pos];
                Pos++;
                ArrayCopy2(Data, Pos, DecompressedData, DataPos, numberOfPlainText);
                DataPos += (int32_t)numberOfPlainText;
                Pos += (int32_t)numberOfPlainText;

                if (DataPos == (int32_t)DecompressedData.size())
                    break;

                int32_t offset = (int32_t)(((Control1 & 0x10) << 12) + (control2 << 8) + (control3) + 1);
                int64_t numberToCopyFromOffset = ((Control1 & 0x0C) << 6) + (control4) + 5;
                OffsetCopy(DecompressedData, offset, DataPos, numberToCopyFromOffset);
                DataPos += (int32_t)numberToCopyFromOffset;

                if (DataPos == (int32_t)DecompressedData.size())
                    break;
            }
            else if (Control1 >= 224 && Control1 <= 251)
            {
                // 0xE0 - 0xFB
                int64_t numberOfPlainText = ((Control1 & 0x1F) << 2) + 4;
                ArrayCopy2(Data, Pos, DecompressedData, DataPos, numberOfPlainText);
                DataPos += (int32_t)numberOfPlainText;
                Pos += (int32_t)numberOfPlainText;

                if (DataPos == (int32_t)DecompressedData.size())
                    break;
            }
            else
            {
                int64_t numberOfPlainText = (Control1 & 0x03);
                ArrayCopy2(Data, Pos, DecompressedData, DataPos, numberOfPlainText);

                DataPos += (int32_t)numberOfPlainText;
                Pos += (int32_t)numberOfPlainText;

                if (DataPos == (int32_t)DecompressedData.size())
                    break;
            }
        }

        return DecompressedData;
    }

    //No data to decompress
    return Data;
}

}}} // namespace FSO::Files::FAR3
