/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
// FSO.SimAntics.Primitives.VMInvokePlugin (primitives/VMInvokePlugin.cs)
#pragma once
#include <cstdint>
#include <unordered_set>
#include "simantics/engine/VMPrimitiveHandler.h"

namespace FSO { namespace SimAntics { namespace Primitives {

class VMInvokePlugin : public Engine::VMPrimitiveHandler
{
public:
    static const std::unordered_set<uint32_t> IdleEvents;

    Engine::VMPrimitiveExitCode Execute(Engine::VMStackFrame* context, Engine::VMPrimitiveOperand* args) override;
};

class VMInvokePluginOperand : public Engine::VMPrimitiveOperand
{
public:
    uint8_t PersonLocal = 0;
    uint8_t ObjectLocal = 0;
    uint8_t EventLocal = 0; //target of event id. values go in temp0
    bool Joinable = false;

    uint32_t PluginID = 0;
    //sign: 0x2a6356a0
    //dancefloor: 0x4a5be8ab
    //pizza: 0xea47ae39

    void Read(const uint8_t* bytes) override;
    void Write(uint8_t* bytes) override;
};

}}}
