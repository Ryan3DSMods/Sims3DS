/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
// Translated from FreeSims sims.files/XA/XAFile.cs
#include "files/XA/XAFile.h"
#include <cstdio>
#include <stdexcept>
#include "core/io.h"

namespace FSO { namespace Files { namespace XA {

const int64_t XAFile::EATable[20] =
{
     0x00000000,
     0x000000F0,
     0x000001CC,
     0x00000188,
     0x00000000,
     0x00000000,
     (int32_t)0xFFFFFF30,
     (int32_t)0xFFFFFF24,
     0x00000000,
     0x00000001,
     0x00000003,
     0x00000004,
     0x00000007,
     0x00000008,
     0x0000000A,
     0x0000000B,
     0x00000000,
     (int32_t)0xFFFFFFFF,
     (int32_t)0xFFFFFFFD,
     (int32_t)0xFFFFFFFC
};
// note: the C# table is long[] holding 0xFFFFFF30 etc. as large positive values; (int)EATable[i] wraps them to
// the negative predictor coefficients (-208, -220), which is what the decoder uses. They are stored pre-wrapped.

XAFile::XAFile(const std::vector<uint8_t>& data)
{
    LoadFile(data);
    DecompressFile();
}

XAFile::XAFile(const std::string& path)
{
    LoadFile(path);
    DecompressFile();
}

void XAFile::LoadFile(const std::string& Path)
{
    FILE* f = std::fopen(Path.c_str(), "rb");
    if (!f) throw std::runtime_error("Could not open file: " + Path);
    std::vector<uint8_t> data;
    std::fseek(f, 0, SEEK_END);
    long n = std::ftell(f);
    std::fseek(f, 0, SEEK_SET);
    if (n > 0) { data.resize((size_t)n); data.resize(std::fread(data.data(), 1, (size_t)n, f)); }
    std::fclose(f);
    LoadFile(data);
}

void XAFile::LoadFile(const std::vector<uint8_t>& Data)
{
    m_Input = Data;
    ReadHeader();
}

void XAFile::ReadHeader()
{
    st::Reader r(m_Input, st::Endian::Little);
    m_ID = r.cstr(4, false);
    m_DecompressedSize = r.u32();
    m_Tag = r.u16();
    m_Channels = r.u16();
    m_SampleRate = r.u32();
    m_AvgByteRate = r.u32();
    m_Align = r.u16();
    m_Bits = r.u16();
    m_ReadPos = r.pos();

    m_DecompressedStream.clear();
    m_DecompressedStream.reserve((size_t)m_DecompressedSize + 44);
    m_CurSampleLeft = m_PrevSampleLeft = m_CurSampleRight = m_PrevSampleRight = 0;
}

void XAFile::Write16(int16_t v)
{
    m_DecompressedStream.push_back((uint8_t)(v & 0xFF));
    m_DecompressedStream.push_back((uint8_t)((v >> 8) & 0xFF));
}

void XAFile::DecompressFile()
{
    uint32_t dwDataSize = /*((m_DecompressedSize - 24) - (m_DecompressedSize - 24) / 15) * 4;*/m_DecompressedSize;
    uint32_t dwFMTSize = 16;
    uint32_t dwRIFFSize = /*dwFMTSize + 8 + dwDataSize + 8 + 4;*/ 8 + 4 + dwFMTSize + 4 + 4 + dwDataSize;

    st::Writer w(st::Endian::Little);
    w.WriteBytes(std::vector<uint8_t>{ 'R', 'I', 'F', 'F' });
    w.WriteUInt32(dwRIFFSize); //Size of file minus this field and the above field.
    w.WriteBytes(std::vector<uint8_t>{ 'W', 'A', 'V', 'E', 'f', 'm', 't', ' ' });
    w.WriteUInt32(dwFMTSize); //Size of WAVEFORMATEX structure (all before 'data').
    w.WriteUInt16(m_Tag);
    w.WriteUInt16(m_Channels);
    w.WriteUInt32(m_SampleRate);
    w.WriteUInt32(m_AvgByteRate);
    w.WriteUInt16(m_Align);
    w.WriteUInt16(m_Bits);
    w.WriteBytes(std::vector<uint8_t>{ 'd', 'a', 't', 'a' });
    w.WriteUInt32(dwDataSize);
    auto& hdr = w.ToArray();
    m_DecompressedStream.assign(hdr.begin(), hdr.end());
    m_DataStart = m_DecompressedStream.size();

    const size_t len = m_Input.size();
    if (m_Channels == 1) //Mono
    {
        while (m_ReadPos < len)
        {
            size_t n = (len - m_ReadPos < 0xF) ? len - m_ReadPos : 0xF; // ReadBytes(0xF): short at EOF
            DecompressMono(m_Input.data() + m_ReadPos, n);
            m_ReadPos += n;
        }
    }
    else if (m_Channels == 2) //Stereo
    {
        while (m_ReadPos < len)
        {
            size_t n = (len - m_ReadPos < 0x1E) ? len - m_ReadPos : 0x1E;
            DecompressStereo(m_Input.data() + m_ReadPos, n);
            m_ReadPos += n;
        }
    }

    // sims: the header promises m_DecompressedSize bytes; the last ADPCM block decodes up to 27 samples
    // of padding past it. Trim to the declared size so a sound does not end with a click of filler.
    size_t want = m_DataStart + (size_t)m_DecompressedSize;
    if (m_DecompressedSize > 0 && m_DecompressedStream.size() > want) m_DecompressedStream.resize(want);
    std::vector<uint8_t>().swap(m_Input); // the compressed input is no longer needed
}

/// <summary>
/// Decompresses a stereo sample.
/// </summary>
/// <param name="InputBuffer">The data containing the stereo sample.</param>
void XAFile::DecompressStereo(const uint8_t* InputBuffer, size_t Length)
{
    //if (InputBuffer.Length != 0x1E) return; //todo, deal with this correctly. Right now stereo audio is kind of totally fucked!
    if (Length < 2) return; // C#: IndexOutOfRange on a truncated block
    uint8_t bInput;
    uint32_t i;
    int32_t c1left, c2left, c1right, c2right, left, right;
    uint8_t dleft, dright;

    bInput = InputBuffer[0];
    c1left = (int32_t)EATable[HINIBBLE(bInput)];   // predictor coeffs for left channel
    c2left = (int32_t)EATable[HINIBBLE(bInput) + 4];
    dleft = (uint8_t)(LONIBBLE(bInput) + 8);   // shift value for left channel

    bInput = InputBuffer[1];
    c1right = (int32_t)EATable[HINIBBLE(bInput)];  // predictor coeffs for right channel
    c2right = (int32_t)EATable[HINIBBLE(bInput) + 4];
    dright = (uint8_t)(LONIBBLE(bInput) + 8);  // shift value for right channel

    for (i = 2; i + 1 < Length; i += 2)
    {
        left = HINIBBLE(InputBuffer[i]);  // HIGHER nibble for left channel
        left = (int32_t)((uint32_t)left << 0x1c) >> dleft;
        left = (left + m_CurSampleLeft * c1left + m_PrevSampleLeft * c2left + 0x80) >> 8;
        left = Clip16BitSample(left);
        m_PrevSampleLeft = m_CurSampleLeft;
        m_CurSampleLeft = left;

        right = HINIBBLE(InputBuffer[i + 1]); // HIGHER nibble for right channel
        right = (int32_t)((uint32_t)right << 0x1c) >> dright;
        right = (right + m_CurSampleRight * c1right + m_PrevSampleRight * c2right + 0x80) >> 8;
        right = Clip16BitSample(right);
        m_PrevSampleRight = m_CurSampleRight;
        m_CurSampleRight = right;

        // Now we've got lCurSampleLeft and lCurSampleRight which form one stereo
        // sample and all is set for the next step...
        //Output((SHORT)lCurSampleLeft,(SHORT)lCurSampleRight); // send the sample to output
        Write16((int16_t)m_CurSampleLeft);
        Write16((int16_t)m_CurSampleRight);

        // now do just the same for LOWER nibbles...
        // note that nubbles for each channel are packed pairwise into one byte

        left = LONIBBLE(InputBuffer[i]);  // LOWER nibble for left channel
        left = (int32_t)((uint32_t)left << 0x1c) >> dleft;
        left = (left + m_CurSampleLeft * c1left + m_PrevSampleLeft * c2left + 0x80) >> 8;
        left = Clip16BitSample(left);
        m_PrevSampleLeft = m_CurSampleLeft;
        m_CurSampleLeft = left;

        right = LONIBBLE(InputBuffer[i + 1]); // LOWER nibble for right channel
        right = (int32_t)((uint32_t)right << 0x1c) >> dright;
        right = (right + m_CurSampleRight * c1right + m_PrevSampleRight * c2right + 0x80) >> 8;
        right = Clip16BitSample(right);
        m_PrevSampleRight = m_CurSampleRight;
        m_CurSampleRight = right;

        // Now we've got lCurSampleLeft and lCurSampleRight which form one stereo
        // sample and all is set for the next step...
        Write16((int16_t)m_CurSampleLeft);
        Write16((int16_t)m_CurSampleRight);
    }
}

/// <summary>
/// Decompresses a mono sample.
/// </summary>
/// <param name="InputBuffer">The data containing the mono sample.</param>
void XAFile::DecompressMono(const uint8_t* InputBuffer, size_t Length)
{
    if (Length < 1) return;
    uint8_t bInput = InputBuffer[0];
    uint32_t i;
    int32_t left, c1left, c2left;
    uint8_t dleft;

    c1left = (int32_t)EATable[HINIBBLE(bInput)];   // predictor coeffs for left channel
    c2left = (int32_t)EATable[HINIBBLE(bInput) + 4];
    dleft = (uint8_t)(LONIBBLE(bInput) + 8);   // shift value for left channel

    for (i = 1; i < 0xF && i < Length; i++)
    {
        left = HINIBBLE(InputBuffer[i]);  // HIGHER nibble for left channel
        left = (int32_t)((uint32_t)left << 0x1c) >> dleft;
        left = (left + m_CurSampleLeft * c1left + m_PrevSampleLeft * c2left + 0x80) >> 8;
        left = Clip16BitSample(left);
        m_PrevSampleLeft = m_CurSampleLeft;
        m_CurSampleLeft = left;

        // Now we've got lCurSampleLeft which is one mono sample and all is set
        // for the next input nibble...
        //Output((SHORT)lCurSampleLeft); // send the sample to output
        Write16((int16_t)m_CurSampleLeft);

        left = LONIBBLE(InputBuffer[i]);  // LOWER nibble for left channel
        left = (int32_t)((uint32_t)left << 0x1c) >> dleft;
        left = (left + m_CurSampleLeft * c1left + m_PrevSampleLeft * c2left + 0x80) >> 8;
        left = Clip16BitSample(left);
        m_PrevSampleLeft = m_CurSampleLeft;
        m_CurSampleLeft = left;

        // Now we've got lCurSampleLeft which is one mono sample and all is set
        // for the next input byte...
        //Output((SHORT)lCurSampleLeft); // send the sample to output
        Write16((int16_t)m_CurSampleLeft);
    }
}

int32_t XAFile::Clip16BitSample(int32_t sample)
{
    if (sample > 32767)
        return 32767;
    else if (sample < -32768)
        return (-32768);
    else
        return sample;
}

}}} // namespace FSO::Files::XA
