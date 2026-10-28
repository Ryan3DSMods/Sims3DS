/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
// FSO.Vitaboy.Appearance / AppearanceBinding (sims.common/rendering/vitaboy/Appearance.cs)
#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include "vitaboy/Binding.h"
#include "vitaboy/AppearanceType.h"
#include "vitaboy/ContentID.h"

namespace FSO { namespace Vitaboy {

class IOProxy;

/// <summary>
/// TypeID and FileID for a binding pointed to by an appearance.
/// </summary>
class AppearanceBinding {
public:
    uint32_t TypeID = 0;
    uint32_t FileID = 0;
    bool HasRealBinding = false; // C#: RealBinding != null
    Binding RealBinding;
};

/// <summary>
/// Represents an appearance for a model (a TS1 "suit").
/// </summary>
class Appearance {
public:
    std::string Name;
    uint32_t ThumbnailTypeID = 0;
    uint32_t ThumbnailFileID = 0;
    std::vector<AppearanceBinding> Bindings;
    int32_t Type = 0; // TS1 suit type (0 normal, 1 censor...), read by ReadBCF

    /// <summary>
    /// Gets the ContentID instance for this appearance.
    /// </summary>
    FSO::Common::Content::ContentID ThumbnailID() const { return FSO::Common::Content::ContentID(ThumbnailTypeID, ThumbnailFileID); }

    /// TS1 suit record (BCF binary or CMX text):
    ///   pstr name, int32 type, int32 zero, uint32 numBindings x (pstr bone, pstr mesh, int32 censor, int32 unknown)
    void ReadBCF(IOProxy& io);
    /// <summary>
    /// Reads a TSO appearance (.apr, big endian).
    /// </summary>
    void Read(const std::vector<uint8_t>& data);
};

}}

// Compatibility umbrella: VM/world code written against the old stub includes only this header
// and uses SimAvatar/Outfit/Skeleton/Animator through it.
#include "vitaboy/Outfit.h"
#include "vitaboy/Skeleton.h"
#include "vitaboy/SimAvatar.h"
#include "vitaboy/Animator.h"
