/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/. 
 */
// Translated from FreeSims sims.files/FAR3/FAR3Exception.cs
#pragma once
#include <stdexcept>
#include <string>

namespace FSO { namespace Files { namespace FAR3 {

/// <summary>
/// Represents an exception thrown by a FAR3Archive instance.
/// </summary>
class FAR3Exception : public std::runtime_error
{
public:
    explicit FAR3Exception(const std::string& Message)
        : std::runtime_error(Message)
    {
    }
};

}}} // namespace FSO::Files::FAR3
