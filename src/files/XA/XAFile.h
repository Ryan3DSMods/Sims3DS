/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
// Translated from FreeSims sims.files/XA/XAFile.cs (Maxis/EA XA ADPCM: "XAI\0" sound/speech, "XAJ\0" music).
#pragma once
#include <cstdint>
#include <string>
#include <vector>

namespace FSO { namespace Files { namespace XA {

enum class SoundType
{
    Mono = 0x00,
    Stereo = 0x01
};

/// <summary>
/// Represents a *.xa file.
/// It is used to store compressed wav data.
/// </summary>
class XAFile
{
private:
    int32_t m_CurSampleLeft = 0;
    int32_t m_PrevSampleLeft = 0;

    int32_t m_CurSampleRight = 0;
    int32_t m_PrevSampleRight = 0;

    // C#: BinaryReader m_Reader / BinaryWriter m_Writer / MemoryStream m_DecompressedStream
    std::vector<uint8_t> m_Input;
    size_t m_ReadPos = 0;
    std::vector<uint8_t> m_DecompressedStream;
    size_t m_DataStart = 0; // sims: offset of the PCM after the RIFF header written by DecompressFile

    // Note that the part of the header from (wTag) until
    //(wBits) is really WAVEFORMATEX structure (the contents of PCM .WAV fmt chunk).
    std::string m_ID = "";        //string ID, which is equal to "XAI\0" (sound/speech) or "XAJ\0" (music).
    uint32_t m_DecompressedSize = 0; //the output size of the audio stream stored in the file (in bytes).
    uint16_t m_Tag = 0;            //seems to be PCM waveformat tag (0x0001). This corresponds to the (decompressed) output audio stream, of course.
    uint16_t m_Channels = 0;       //number of channels for the file.
    uint32_t m_SampleRate = 0;       //sample rate for the file.
    uint32_t m_AvgByteRate = 0;      //average byte rate for the file (equal to (dwSampleRate)*(wAlign)). Note that this also corresponds to the decompressed output audio stream.
    uint16_t m_Align = 0;          //the sample align value for the file (equal to (wBits/8)*(wChannels)). Again, this corresponds to the decompressed output audio stream.
    uint16_t m_Bits = 0;           //resolution of the file (8 (8-bit), 16 (16-bit), etc.).

    void ReadHeader();
    void Write16(int16_t v);
    void DecompressStereo(const uint8_t* InputBuffer, size_t Length);
    void DecompressMono(const uint8_t* InputBuffer, size_t Length);
    int32_t Clip16BitSample(int32_t sample);
    uint8_t HINIBBLE(uint8_t B) { return (uint8_t)(((B) >> 4) & 0x0F); }
    uint8_t LONIBBLE(uint8_t B) { return (uint8_t)((B) & 0x0F); }
    static const int64_t EATable[20];

public:
    /// <summary>
    /// The decompressed wave data as a byte array (a complete RIFF/WAVE file).
    /// Will not contain any data unless DecompressFile() has been called!
    /// </summary>
    const std::vector<uint8_t>& DecompressedData() const { return m_DecompressedStream; }

    // sims: direct access to the decoded PCM (s16 LE, interleaved) and its format, so the mixer does not
    // have to parse the RIFF header again.
    const int16_t* Samples() const { return (const int16_t*)(m_DecompressedStream.data() + m_DataStart); }
    size_t SampleCount() const { return (m_DecompressedStream.size() - m_DataStart) / 2; }
    uint16_t Channels() const { return m_Channels; }
    uint32_t SampleRate() const { return m_SampleRate; }
    const std::string& ID() const { return m_ID; }

    explicit XAFile(const std::vector<uint8_t>& data);
    explicit XAFile(const std::string& path);

    /// <summary>
    /// Loads a *.xa file, setting things up for decompression.
    /// Should always be called before DecompressFile().
    /// </summary>
    /// <param name="Path">The path to the *.xa file to load.</param>
    void LoadFile(const std::string& Path);

    /// <summary>
    /// Loads an *.xa file, setting things up for decompression.
    /// Should always be called before DecompressFile().
    /// </summary>
    /// <param name="Data">The data of the *.xa file to process.</param>
    void LoadFile(const std::vector<uint8_t>& Data);

    /// <summary>
    /// Decompresses the sounddata and stores it in the the decompressed stream in this class.
    /// </summary>
    void DecompressFile();
};

}}} // namespace FSO::Files::XA
