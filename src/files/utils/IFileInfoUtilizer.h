/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
// Translated from FreeSims sims.files/utils/IFileInfoUtilizer.cs
#pragma once
#include <string>

namespace FSO { namespace Files { namespace Utils {

class IFileInfoUtilizer
{
public:
    virtual ~IFileInfoUtilizer() {}
    virtual void SetFilename(const std::string& filename) = 0;
};

}}} // namespace FSO::Files::Utils
