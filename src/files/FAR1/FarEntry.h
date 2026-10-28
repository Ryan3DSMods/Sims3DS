/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/. 
 */
// Translated from FreeSims sims.files/FAR1/FarEntry.cs
#pragma once
#include <cstdint>
#include <string>

namespace FSO { namespace Files { namespace FAR1 {

/// <summary>
/// Represents an entry in a FAR1 archive.
/// </summary>
class FarEntry
{
public:
    //Decompressed data size - A 4-byte unsigned integer specifying the uncompressed size of the file.
    int32_t DataLength = 0;
    //A 4-byte unsigned integer specifying the compressed size of the file; if this and the previous field are the same, 
    //the file is considered uncompressed. (It is the responsibility of the archiver to only store data compressed when 
    //its size is less than the size of the original data.) Note that The Sims 1 does not actually support any form 
    //of compression.
    int32_t DataLength2 = 0;
    //A 4-byte unsigned integer specifying the offset of the file from the beginning of the archive.
    int32_t DataOffset = 0;
    //A 4-byte unsigned integer specifying the length of the filename field that follows.
    int16_t FilenameLength = 0;
    //Filename - The name of the archived file; size depends on the previous field.
    std::string Filename;
};

}}} // namespace FSO::Files::FAR1
