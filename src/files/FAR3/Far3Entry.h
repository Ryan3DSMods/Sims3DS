/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/. 
 */
// Translated from FreeSims sims.files/FAR3/Far3Entry.cs
#pragma once
#include <cstdint>
#include <string>

namespace FSO { namespace Files { namespace FAR3 {

/// <summary>
/// Represents an entry in a FAR3 archive.
/// </summary>
class Far3Entry
{
public:
    //A 4-byte unsigned integer specifying the uncompressed size of the file.
    uint32_t DecompressedFileSize = 0;
    // A 3-byte unsigned integer specifying the compressed size of the file (including 
    //the Persist header); if the data is raw, this field is ignored (though TSO's game 
    //files have this set to the same first three bytes as the previous field).
    uint32_t CompressedFileSize = 0;
    //Data type - A single byte used to describe what type of data is pointed to by the Data offset field. 
    //The value can be 0x80 to denote that the data is a Persist container or 0x00 to denote that it is raw data.
    uint8_t DataType = 0;
    //A 4-byte unsigned integer specifying the offset of the file from the beginning of the archive.
    uint32_t DataOffset = 0;
    //A byte (can be either 0 or 1) specifying if this file is compressed.
    uint8_t IsCompressed = 0;
    //A byte specifying the number of files this time has been accessed?
    uint8_t AccessNumber = 0;
    //A 2-byte unsigned integer specifying the length of the filename field.
    uint16_t FilenameLength = 0;
    //A 4-byte integer describing what type of file is held.
    uint32_t TypeID = 0;
    //A 4-byte ID assigned to the file which, together with the Type ID, is assumed to be unique all throughout the game.
    uint32_t FileID = 0;
    //The name of the archived file; size depends on the filename length field.
    std::string Filename;
};

}}} // namespace FSO::Files::FAR3
