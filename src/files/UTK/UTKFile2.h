/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
// Translated from FreeSims sims.files/UTK/UTKFile2.cs (Maxis UTalk "UTM0" speech codec). The Sims 1 ships its
// speech as XA; UTK is translated for completeness (TSO / mods).
#pragma once
#include <cstdint>
#include <string>
#include <vector>

namespace FSO { namespace Files { namespace UTK {

/// <summary>
/// Represents a *.UTK file.
/// It is used to store compressed wav data.
/// </summary>
class UTKFile2
{
public:
    static const float UTKCosine[64];

private:
    static const uint8_t UTKCodebook[512];
    static const uint8_t m_UTKCodeSkips[29];

    std::string m_ID = "";        //A 4-byte string identifier equal to "UTM0".
    uint32_t m_DecompressedSize = 0; //The decompressed size of the audio stream.
    uint32_t m_WaveFormatXSize = 0;  //The size in bytes of the WAVEFORMATEX structure to follow; must be 20.

    //WAVEFORMATEX
    uint16_t m_FormatTag = 0;
    uint16_t m_NumChannels = 0;
    uint32_t m_SamplesPerSec = 0;
    uint32_t m_AvgBytesPerSec = 0;
    uint16_t m_BlockAlign = 0;
    uint16_t m_BitsPerSample = 0;
    uint32_t m_AppendSize = 0;

    std::vector<uint8_t> m_DecompressedStream;
    std::vector<uint8_t> m_Input;  // C#: BinaryReader m_Reader
    size_t m_ReadPos = 0;
    int32_t m_UnreadBitsValue = 0, m_UnreadBitsCount = 0;
    bool m_HalvedExcitation = false;
    uint8_t m_VoicedThreshold = 0;
    float m_InnovationPower[64] = {};
    float m_RC[12] = {};
    float m_History[12] = {};
    float m_DecompressedFrame[756] = {};

    void ReadHeader();
    uint8_t ReadBits(uint8_t Bits);
    int32_t Lesser(int32_t A, int32_t B) { return (A < B ? A : B); }
    void DecodeFrame();
    void GenerateExcitation(int32_t Offset, float* Excitation, bool Voiced, int32_t Interval);
    void Synthesize(int32_t Sample, int32_t Samples);
    void RCtoLPC(const float* RC, float* LPC);
    void Write16(uint16_t v) { m_DecompressedStream.push_back((uint8_t)v); m_DecompressedStream.push_back((uint8_t)(v >> 8)); }
    void Write32(uint32_t v) { Write16((uint16_t)v); Write16((uint16_t)(v >> 16)); }

public:
    /// <summary>
    /// Returns a decompressed wav stream.
    /// </summary>
    const std::vector<uint8_t>& DecompressedWav() const { return m_DecompressedStream; }
    uint16_t Channels() const { return m_NumChannels; }   // sims
    uint32_t SampleRate() const { return m_SamplesPerSec; } // sims

    /// <summary>
    /// Creates a new UTKFile2 instance.
    /// </summary>
    /// <param name="FileData">The file data to read from.</param>
    explicit UTKFile2(const std::vector<uint8_t>& FileData);

    /// <summary>
    /// Creates a new UTKFile2 instance.
    /// </summary>
    /// <param name="Filepath">The path to a *.utk file to read from.</param>
    explicit UTKFile2(const std::string& Filepath);

    /// <summary>
    /// Clamps a value between an upper and lower bound.
    /// </summary>
    template <typename T>
    static T Clamp(T value, T max, T min)
    {
        T result = value;
        if (value < max)
            result = max;
        if (value > min)
            result = min;
        return result;
    }

    /// <summary>
    /// Decodes the UTK data in this file.
    /// </summary>
    void UTKDecode();
};

}}} // namespace FSO::Files::UTK
