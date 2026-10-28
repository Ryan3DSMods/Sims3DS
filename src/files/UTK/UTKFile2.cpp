/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
// Translated from FreeSims sims.files/UTK/UTKFile2.cs
#include "files/UTK/UTKFile2.h"
#include <cmath>
#include <cstdio>
#include <cstring>
#include <stdexcept>

namespace FSO { namespace Files { namespace UTK {

const float UTKFile2::UTKCosine[64] = {
            0.0f, -.99677598476409912109375f, -.99032700061798095703125f, -.983879029750823974609375f, -.977430999279022216796875f,
            -.970982015132904052734375f, -.964533984661102294921875f, -.958085000514984130859375f, -.9516370296478271484375f,
            -.930754005908966064453125f, -.904959976673126220703125f, -.879167020320892333984375f, -.853372991085052490234375f,
            -.827579021453857421875f, -.801786005496978759765625f, -.775991976261138916015625f, -.75019800662994384765625f,
            -.724404990673065185546875f, -.6986110210418701171875f, -.6706349849700927734375f, -.61904799938201904296875f,
            -.567460000514984130859375f, -.515873014926910400390625f, -.4642859995365142822265625f, -.4126980006694793701171875f,
            -.361110985279083251953125f, -.309523999691009521484375f, -.257937014102935791015625f, -.20634900033473968505859375f,
            -.1547619998455047607421875f, -.10317499935626983642578125f, -.05158700048923492431640625f,
            0.0f,
            +.05158700048923492431640625f, +.10317499935626983642578125f, +.1547619998455047607421875f, +.20634900033473968505859375f,
            +.257937014102935791015625f, +.309523999691009521484375f, +.361110985279083251953125f, +.4126980006694793701171875f,
            +.4642859995365142822265625f, +.515873014926910400390625f, +.567460000514984130859375f, +.61904799938201904296875f,
            +.6706349849700927734375f, +.6986110210418701171875f, +.724404990673065185546875f, +.75019800662994384765625f,
            +.775991976261138916015625f, +.801786005496978759765625f, +.827579021453857421875f, +.853372991085052490234375f,
            +.879167020320892333984375f, +.904959976673126220703125f, +.930754005908966064453125f, +.9516370296478271484375f,
            +.958085000514984130859375f, +.964533984661102294921875f, +.970982015132904052734375f, +.977430999279022216796875f,
            +.983879029750823974609375f, +.99032700061798095703125f, +.99677598476409912109375f
        };

const uint8_t UTKFile2::UTKCodebook[512] = {
            4,  6,  5,  9,  4,  6,  5, 13,  4,  6,  5, 10,  4,  6,  5, 17,
            4,  6,  5,  9,  4,  6,  5, 14,  4,  6,  5, 10,  4,  6,  5, 21,
            4,  6,  5,  9,  4,  6,  5, 13,  4,  6,  5, 10,  4,  6,  5, 18,
            4,  6,  5,  9,  4,  6,  5, 14,  4,  6,  5, 10,  4,  6,  5, 25,
            4,  6,  5,  9,  4,  6,  5, 13,  4,  6,  5, 10,  4,  6,  5, 17,
            4,  6,  5,  9,  4,  6,  5, 14,  4,  6,  5, 10,  4,  6,  5, 22,
            4,  6,  5,  9,  4,  6,  5, 13,  4,  6,  5, 10,  4,  6,  5, 18,
            4,  6,  5,  9,  4,  6,  5, 14,  4,  6,  5, 10,  4,  6,  5,  0,
            4,  6,  5,  9,  4,  6,  5, 13,  4,  6,  5, 10,  4,  6,  5, 17,
            4,  6,  5,  9,  4,  6,  5, 14,  4,  6,  5, 10,  4,  6,  5, 21,
            4,  6,  5,  9,  4,  6,  5, 13,  4,  6,  5, 10,  4,  6,  5, 18,
            4,  6,  5,  9,  4,  6,  5, 14,  4,  6,  5, 10,  4,  6,  5, 26,
            4,  6,  5,  9,  4,  6,  5, 13,  4,  6,  5, 10,  4,  6,  5, 17,
            4,  6,  5,  9,  4,  6,  5, 14,  4,  6,  5, 10,  4,  6,  5, 22,
            4,  6,  5,  9,  4,  6,  5, 13,  4,  6,  5, 10,  4,  6,  5, 18,
            4,  6,  5,  9,  4,  6,  5, 14,  4,  6,  5, 10,  4,  6,  5,  2,
            4, 11,  7, 15,  4, 12,  8, 19,  4, 11,  7, 16,  4, 12,  8, 23,
            4, 11,  7, 15,  4, 12,  8, 20,  4, 11,  7, 16,  4, 12,  8, 27,
            4, 11,  7, 15,  4, 12,  8, 19,  4, 11,  7, 16,  4, 12,  8, 24,
            4, 11,  7, 15,  4, 12,  8, 20,  4, 11,  7, 16,  4, 12,  8,  1,
            4, 11,  7, 15,  4, 12,  8, 19,  4, 11,  7, 16,  4, 12,  8, 23,
            4, 11,  7, 15,  4, 12,  8, 20,  4, 11,  7, 16,  4, 12,  8, 28,
            4, 11,  7, 15,  4, 12,  8, 19,  4, 11,  7, 16,  4, 12,  8, 24,
            4, 11,  7, 15,  4, 12,  8, 20,  4, 11,  7, 16,  4, 12,  8,  3,
            4, 11,  7, 15,  4, 12,  8, 19,  4, 11,  7, 16,  4, 12,  8, 23,
            4, 11,  7, 15,  4, 12,  8, 20,  4, 11,  7, 16,  4, 12,  8, 27,
            4, 11,  7, 15,  4, 12,  8, 19,  4, 11,  7, 16,  4, 12,  8, 24,
            4, 11,  7, 15,  4, 12,  8, 20,  4, 11,  7, 16,  4, 12,  8,  1,
            4, 11,  7, 15,  4, 12,  8, 19,  4, 11,  7, 16,  4, 12,  8, 23,
            4, 11,  7, 15,  4, 12,  8, 20,  4, 11,  7, 16,  4, 12,  8, 28,
            4, 11,  7, 15,  4, 12,  8, 19,  4, 11,  7, 16,  4, 12,  8, 24,
            4, 11,  7, 15,  4, 12,  8, 20,  4, 11,  7, 16,  4, 12,  8,  3
        };

const uint8_t UTKFile2::m_UTKCodeSkips[29] = { 8, 7, 8, 7, 2, 2, 2, 3, 3, 4, 4, 3, 3, 5, 5, 4, 4, 6, 6, 5, 5, 7, 7, 6, 6, 8, 8, 7, 7 };

UTKFile2::UTKFile2(const std::vector<uint8_t>& FileData)
{
    m_Input = FileData;
    ReadHeader();
}

UTKFile2::UTKFile2(const std::string& Filepath)
{
    FILE* f = std::fopen(Filepath.c_str(), "rb");
    if (!f) throw std::runtime_error("Could not open file: " + Filepath);
    std::fseek(f, 0, SEEK_END);
    long n = std::ftell(f);
    std::fseek(f, 0, SEEK_SET);
    if (n > 0) { m_Input.resize((size_t)n); m_Input.resize(std::fread(m_Input.data(), 1, (size_t)n, f)); }
    std::fclose(f);
    ReadHeader();
}

/// <summary>
/// Reads the header of a .utk file.
/// </summary>
void UTKFile2::ReadHeader()
{
    if (m_Input.size() < 32) throw std::runtime_error("UTK: file too short");
    auto u16 = [&](size_t o) { return (uint16_t)(m_Input[o] | (m_Input[o + 1] << 8)); };
    auto u32 = [&](size_t o) { return (uint32_t)m_Input[o] | ((uint32_t)m_Input[o + 1] << 8) | ((uint32_t)m_Input[o + 2] << 16) | ((uint32_t)m_Input[o + 3] << 24); };
    m_ID = std::string((const char*)m_Input.data(), 4);
    m_DecompressedSize = u32(4);
    m_WaveFormatXSize = u32(8);
    m_FormatTag = u16(12);
    m_NumChannels = u16(14);
    m_SamplesPerSec = u32(16);
    m_AvgBytesPerSec = u32(20);
    m_BlockAlign = u16(24);
    m_BitsPerSample = u16(26);
    m_AppendSize = u32(28);
    m_ReadPos = 32;

    m_DecompressedStream.clear();

    //Write wav-header.
    uint32_t dwDataSize = m_DecompressedSize;
    uint32_t dwFMTSize = 16;
    uint32_t dwRIFFSize = 36 + dwDataSize;

    for (char c : std::string("RIFF")) m_DecompressedStream.push_back((uint8_t)c);
    Write32(dwRIFFSize); //Size of file minus this field and the above field.
    for (char c : std::string("WAVEfmt ")) m_DecompressedStream.push_back((uint8_t)c);
    Write32(dwFMTSize); //Size of WAVEFORMATEX structure (all the data that comes after this field).
    Write16(m_FormatTag);
    Write16(m_NumChannels);
    Write32(m_SamplesPerSec);
    Write32(m_AvgBytesPerSec);
    Write16(m_BlockAlign);
    Write16(m_BitsPerSample);
    for (char c : std::string("data")) m_DecompressedStream.push_back((uint8_t)c);
    Write32(dwDataSize);

    m_UnreadBitsValue = (m_ReadPos < m_Input.size()) ? m_Input[m_ReadPos++] : 0;
    m_UnreadBitsCount = 8;
    m_HalvedExcitation = (ReadBits(1) != 0);
    m_VoicedThreshold = (uint8_t)(32 - ReadBits(4));

    m_InnovationPower[0] = (float)((ReadBits(4) + 1) * 8); //Significand.

    float Base = 1.04f + (float)(ReadBits(6)) / 1000.0f;
    for (int i = 1; i < 64; i++)
        m_InnovationPower[i] = m_InnovationPower[i - 1] * Base;
}

uint8_t UTKFile2::ReadBits(uint8_t Bits)
{
    uint8_t Value = (uint8_t)(m_UnreadBitsValue & (255 >> (8 - Bits)));
    m_UnreadBitsValue >>= Bits;
    m_UnreadBitsCount -= Bits;

    if ((m_UnreadBitsCount < 8) && (m_ReadPos < m_Input.size()))
    {
        m_UnreadBitsValue |= m_Input[m_ReadPos++] << m_UnreadBitsCount;
        m_UnreadBitsCount += 8;
    }

    return Value;
}

/// <summary>
/// Decodes the UTK data in this file.
/// </summary>
void UTKFile2::UTKDecode()
{
    uint32_t Frames = (m_BlockAlign != 0) ? m_DecompressedSize / m_BlockAlign : 0;
    m_DecompressedStream.reserve(m_DecompressedStream.size() + (size_t)Frames * 2);

    while (Frames > 0)
    {
        int32_t BlockSize = Lesser((int32_t)Frames, 432);
        DecodeFrame();

        for (int32_t i = 0; i < BlockSize; i++)
        {
            int32_t Value = (int32_t)std::lround(m_DecompressedFrame[324 + i]); // C# Math.Round (banker's; ties are rare)
            Value = Clamp<int32_t>(Value, -32768, 32767);
            Write16((uint16_t)Value);
        }

        Frames -= (uint32_t)BlockSize;
    }

    std::vector<uint8_t>().swap(m_Input); // m_Reader.Close()
}

void UTKFile2::DecodeFrame()
{
    float Excitation[118] = {}; //includes 5 0-valued samples to both the left and the right.
    float RCDelta[12] = {};
    bool Voiced = false;

    for (int i = 0; i < 12; i++)
    {
        uint8_t index = ReadBits((uint8_t)((i < 4) ? 6 : 5));

        if (i == 0 && index < m_VoicedThreshold)
            Voiced = true;

        RCDelta[i] = (UTKCosine[index + ((i < 4) ? 0 : 16)] - m_RC[i]) / 4.0f;
    }

    for (int i = 0; i < 4; i++)
    {
        int32_t Phase = ReadBits(8);

        if (i == 0 && Phase > 216)
            Phase = 216;

        float PitchGain = (float)(ReadBits(4)) / 15.0f;
        float InnovationGain = m_InnovationPower[ReadBits(6)];

        if (m_HalvedExcitation == false)
        {
            GenerateExcitation(5, Excitation, Voiced, 1);
        }
        else
        {
            //Fill the excitation window with half as many samples and interpolate the rest.
            int32_t Alignment = ReadBits(1); //whether to fill the even or odd samples.
            bool FillWithZero = (ReadBits(1) != 0);
            int32_t Offset = 5 + (1 - Alignment);

            GenerateExcitation(5 + Alignment, Excitation, Voiced, 2);

            if (FillWithZero)
            {
                for (int32_t j = Offset; j < Offset + 108; j += 2)
                    Excitation[j] = 0.0f;
            }
            else
            {
                //Use sinc interpolation with 6 neighboring samples.
                for (int32_t j = Offset; j < Offset + 108; j += 2)
                    Excitation[j] =
                      (Excitation[j - 1] + Excitation[j + 1]) * .5973859429f
                    - (Excitation[j - 3] + Excitation[j + 3]) * .1145915613f
                    + (Excitation[j - 5] + Excitation[j + 5]) * .0180326793f;

                InnovationGain /= 2.0f;
            }
        }

        for (int j = 0; j < 108; j++)
            m_DecompressedFrame[324 + 108 * i + j] = InnovationGain * Excitation[5 + j] + PitchGain * m_DecompressedFrame[108 * i + j + (216 - Phase)];
    }

    std::memmove(m_DecompressedFrame, m_DecompressedFrame + 324 + 108, 324 * sizeof(float)); // Array.Copy

    for (int i = 0; i < 4; i++)
    {
        //Linearly interpolate the reflection coefficients for the current subframe.
        for (int j = 0; j < 12; j++)
            m_RC[j] += RCDelta[j];

        Synthesize(i * 12, (i != 3) ? 12 : 396);
    }
}

void UTKFile2::GenerateExcitation(int32_t Offset, float* Excitation, bool Voiced, int32_t Interval)
{
    if (Voiced)
    {
        int32_t Table = 0;
        int32_t i = Offset;

        while (i < Offset + 108)
        {
            uint8_t code = UTKCodebook[(Table << 8) | (m_UnreadBitsValue & 0xFF)];
            //Table = (code < 2 || code > 8);
            Table = (code < 2 || code > 8) ? 1 : 0;
            ReadBits(m_UTKCodeSkips[code]);

            if (code >= 4)
            {
                //Fill a sample with a value specified by the code; magnitude is limited to 6.0
                Excitation[i] = (float)((code - 1) / 4);
                if ((code & 1) != 0)
                    Excitation[i] *= -1.0f;

                i += Interval;
            }
            else if (code >= 2)
            {
                //Fill between 7 and 70 samples with 0s
                int32_t x = ReadBits(6) + 7;
                x = Lesser(x, (Offset + 108 - i) / Interval);

                while (x > 0)
                {
                    Excitation[i] = 0.0f;

                    i += Interval;
                    x--;
                }
            }
            else
            {
                //Fill a sample with a custom value with magnitude >= 7.0
                Excitation[i] = 7.0f;
                while (ReadBits(1) != 0)
                    Excitation[i]++;

                if (ReadBits(1) == 0)
                    Excitation[i] *= -1.0f;

                i += Interval;
            }
        }
    }
    else
    {
        //Unvoiced: restrict all samples to 0.0, -2.0, or +2.0 without using the codebook
        for (int32_t i = Offset; i < Offset + 108; i += Interval)
        {
            if (ReadBits(1) == 0) Excitation[i] = 0.0f;
            else if (ReadBits(1) == 0) Excitation[i] = -2.0f;
            else Excitation[i] = 2.0f;
        }
    }
}

void UTKFile2::Synthesize(int32_t Sample, int32_t Samples)
{
    float LPC[12] = {};
    int32_t offset = -1;
    RCtoLPC(m_RC, LPC);

    while (Samples > 0)
    {
        for (int i = 0; i < 12; i++)
        {
            if (++offset == 12) offset = 0;
            m_DecompressedFrame[324 + Sample] += LPC[i] * m_History[offset];
        }

        m_History[offset--] = m_DecompressedFrame[324 + Sample++];
        Samples--;
    }
}

void UTKFile2::RCtoLPC(const float* RC, float* LPC)
{
    int i, j;
    float RCTemp[12] = {}, LPCTemp[12] = {};
    RCTemp[0] = 1.0f;
    std::memcpy(RCTemp + 1, RC, 11 * sizeof(float));

    for (i = 0; i < 12; i++)
    {
        LPC[i] = 0.0f;

        for (j = 11; j >= 0; j--)
        {
            LPC[i] -= RC[j] * RCTemp[j];
            if (j != 11)
                RCTemp[j + 1] = RCTemp[j] + RC[j] * LPC[i];
        }

        RCTemp[0] = LPCTemp[i] = LPC[i];

        for (j = 0; j < i; j++)
            LPC[i] -= LPCTemp[i - j - 1] * LPC[j];
    }
}

}}} // namespace FSO::Files::UTK
