/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
// FSO.SimAntics.Primitives.VMTS1MakeNewCharacter (FreeSO tso.simantics/Primitives/VMTS1MakeNewCharacter.cs, MPL-2.0)
// TS1(sims): primitive 19 "Make New Character" - FreeSims has no TS1 version. Makes a character file
// (UserData/Characters/User#####.iff, cloned from TemplatePerson / TemplateDog / TemplateCat with
// content/TS1CharacterFiles), adds its neighbour record and puts the new neighbour id in the stack object; the
// caller then creates the Sim with Create Object Instance "use neighbor" (flag 4) and adds it to the family
// (Generic Sims Call 4). Used by: Baby 'create child' (a baby grows up), PetMaternityBox (puppies/kittens), the
// pet store pens / mailbox debug "Buy a Pet", ChemistrySet 'make clone', Charms "Send Clone to Work/School" and the
// Makin' Magic toad prince.
//
// Operand: ColorLocal, AgeLocal, GenderLocal (local indices), AvatarType (0 person, 1 dog, 2 cat).
//  * skin colour = person data 60 values (1 lgt, 2 drk, 3 med; the baby passes its own); age < 18 = child;
//    gender 0 male / 1 female. Head and body are a random everyday outfit of that kind (content/TS1Clothing),
//    the body strings are laid out as Create-A-Sim writes them (content/TS1SimDesc).
//  * age < 0: a clone of the stack object (or the caller) - same body strings, sex, age and person data.
//  * pets: coat number = stack object attribute 8 (the pen / maternity box's choice) among the coats.
#pragma once
#include <cstdint>
#include "simantics/engine/VMPrimitiveHandler.h"
#include "simantics/engine/VMPrimitiveOperand.h"

namespace FSO { namespace SimAntics { namespace Primitives {

class VMTS1MakeNewCharacter : public Engine::VMPrimitiveHandler {
public:
    Engine::VMPrimitiveExitCode Execute(Engine::VMStackFrame* context, Engine::VMPrimitiveOperand* args) override;
};

class VMTS1MakeNewCharacterOperand : public Engine::VMPrimitiveOperand {
public:
    uint8_t ColorLocal = 0;
    uint8_t AgeLocal = 0;
    uint8_t GenderLocal = 0;
    uint8_t AvatarType = 0;
    void Read(const uint8_t* bytes) override;
    void Write(uint8_t* bytes) override;
};

}}}
