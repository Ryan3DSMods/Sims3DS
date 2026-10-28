/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
// FSO.SimAntics.Engine.VMTranslator
#pragma once
#include <memory>
#include "simantics/_files_contract.h"
#include "simantics/engine/translator/VMRoutine.h"
namespace FSO { namespace SimAntics {
class VM;
namespace Engine {
class VMTranslator {
public:
    static std::unique_ptr<VMRoutine> Assemble(FSO::SimAntics::VM* vm, FSO::Files::Formats::IFF::Chunks::BHAV* bhav);
};
}}}
