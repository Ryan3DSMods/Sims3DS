/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/. 
 */
// Translated from FreeSims sims.files/FAR3/Decompresser.cs
#pragma once
#include <cstdint>
#include <vector>

namespace FSO { namespace Files { namespace FAR3 {

/// <summary>
/// Represents a decompresser that can decompress files in a FAR3
/// archive. If you have some kind of need to understand this code, go to:
/// http://wiki.niotso.org/RefPack
/// The code in this class was ported from DBPF4J:
/// http://sc4dbpf4j.cvs.sourceforge.net/viewvc/sc4dbpf4j/DBPF4J/
/// </summary>
class Decompresser
{
private:
    int64_t m_CompressedSize = 0;
    int64_t m_DecompressedSize = 0;

public:
    int64_t DecompressedSize() const { return m_DecompressedSize; }
    void DecompressedSize(int64_t value) { m_DecompressedSize = value; }

    int64_t CompressedSize() const { return m_CompressedSize; }
    void CompressedSize(int64_t value) { m_CompressedSize = value; }

private:
    /// <summary>
    ///  Copies data from source to destination array.
    ///  The copy is byte by byte from srcPos to destPos and given length.
    /// </summary>
    static void ArrayCopy2(const std::vector<uint8_t>& Src, int32_t SrcPos, std::vector<uint8_t>& Dest, int32_t DestPos, int64_t Length);

    /// <summary>
    /// Copies data from array at destPos-srcPos to array at destPos.
    /// </summary>
    static void OffsetCopy(std::vector<uint8_t>& array, int32_t srcPos, int32_t destPos, int64_t length);

public:
    /// <summary>
    /// Compresses data and returns it as an array of bytes.
    /// Assumes that the array of bytes passed contains 
    /// uncompressed data.
    /// </summary>
    std::vector<uint8_t> Compress(const std::vector<uint8_t>& Data);

    /// <summary>
    /// Decompresses data and returns it as an
    /// uncompressed array of bytes.
    /// </summary>
    std::vector<uint8_t> Decompress(const std::vector<uint8_t>& Data);
};

}}} // namespace FSO::Files::FAR3
