/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
// sims — the heads and bodies Create-A-Sim cycles through (see README.md).
// TS1(sims): moved from src/app/cas/CASCatalog.h (shared with Make New Character); app/cas/CASCatalog.h forwards.
//
// TS1(sims): The Sims lists skins by their texture file names; a texture belongs to the CAS catalogue when it
// follows the CAS naming scheme and a mesh of the same family exists:
//   heads   c<###><m|f|u><a|c><lgt|med|drk>_<name>.bmp            mesh c<###><g><a>_<name>      (suit in a BCF/CMX)
//   bodies  b<###><m|f|u><a|c><fat|fit|skn|chd><lgt|med|drk>_<name>.bmp   mesh b<###><g><a><type>_<name>
// NPC uniforms (maid, police, pizza guy...) use other names and so never appear, exactly as in the game.
// Sources: everything the avatar content provider indexes (GameData Textures.far/Animation.far/Skins, every
// expansion pack FAR, ExpansionShared/SkinsBuy, bundled Downloads) — so EP and download heads are included.
#pragma once
#include <map>
#include <set>
#include <string>
#include <vector>
#include "content/TS1SimDesc.h"

namespace FSO { namespace Content { namespace TS1 { namespace CAS {

class CASCatalog {
public:
    /// Scans the TS1 avatar content (TS1AvatarContent must have its base path set). Returns false without data.
    bool Build();
    bool Built() const { return _Built; }

    const std::vector<CASOutfit>& Heads(CASAge a, CASGender g, CASSkin s) const { return _Heads[Key(a, g, s)]; }
    const std::vector<CASOutfit>& Bodies(CASAge a, CASGender g, CASSkin s) const { return _Bodies[Key(a, g, s)]; }
    /// Index of the outfit with this texture (or the same name in another skin tone), -1 if absent.
    static int Find(const std::vector<CASOutfit>& list, const CASOutfit& o);
    /// Equivalent outfit of another skin tone (same number and name, e.g. c001malgt_ross -> c001madrk_ross).
    static int FindSameStyle(const std::vector<CASOutfit>& list, const CASOutfit& o);
    /// Removes an entry whose mesh could not be loaded (the preview found it broken).
    void MarkBroken(const CASOutfit& o);

    size_t HeadCount() const;
    size_t BodyCount() const;

private:
    static int Key(CASAge a, CASGender g, CASSkin s) { return (int)a * 6 + (int)g * 3 + (int)s; }
    bool _Built = false;
    mutable std::vector<CASOutfit> _Heads[12], _Bodies[12];
};

}}}}
