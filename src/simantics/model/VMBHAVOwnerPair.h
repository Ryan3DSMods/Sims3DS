/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
// FSO.SimAntics.Model.VMBHAVOwnerPair
#pragma once
#include "simantics/_files_contract.h"
#include "content/ContentManager.h"
namespace FSO { namespace SimAntics { namespace Model {
class VMBHAVOwnerPair {
public:
    FSO::Files::Formats::IFF::Chunks::BHAV* bhav;
    FSO::Content::GameObject* owner;
    VMBHAVOwnerPair(FSO::Files::Formats::IFF::Chunks::BHAV* b, FSO::Content::GameObject* o) : bhav(b), owner(o) {}
};
}}}
