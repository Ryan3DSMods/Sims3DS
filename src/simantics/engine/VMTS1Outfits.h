/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
// TS1(sims): FreeSO VMTS1PurchasableOutfitHelper on VM avatars (Generic Sims Calls 24/29/30/31: the Hot Date
// clothing racks, the dressers' "Change Normal Outfit", the Superstar / Unleashed racks, pet coats when kittens and
// puppies are born). The outfit lists themselves are content/TS1Clothing.
//
// Front-end API (the "Clothing" dialog, Behavior.iff STR#217 dialog type 6, shown by dressers and racks): list the
// choices with ValidOutfits(avatar, type) - the dialog's answer is the index; GSC 24 then applies it. The texture of
// each entry (the part after "BODY=") is the thumbnail the original shows.
#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include "content/TS1Clothing.h"

namespace FSO { namespace SimAntics {
class VMAvatar;
class VMContext;
namespace Engine { namespace Utils {

class VMTS1Outfits {
public:
    /// Outfit type: 0 daywear, 1 formal, 2 swimwear, 3 pyjamas, 4 winter, 5 high fashion (pets: their coat, any type).
    static std::vector<FSO::Content::TS1::TS1Clothing::Outfit> ValidOutfits(VMAvatar* avatar, int16_t outfitType);
    static int16_t GetSuitIndex(VMAvatar* avatar, int16_t outfitType);
    static int16_t CountSuits(VMAvatar* avatar, int16_t outfitType);
    /// Applies outfit `index`, saves the body strings (character STR#200 in memory, marked dirty, written to the
    /// character file), re-dresses a pet at once. Returns the previous index (-1 unknown, -2 nothing changed).
    static int16_t SetSuit(VMAvatar* avatar, int16_t outfitType, int16_t index);
    /// Writes new body strings into the avatar's character (STR#OBJD.BodyStringID) and its character file.
    static void SaveBodyStrings(VMAvatar* avatar, const std::vector<std::string>& bodyStrings);
    /// Generic Sims Call 39 "child to adult" (Makin' Magic: the growth charm, the "Kid to Adult" spell). The child
    /// becomes an adult of the same sex and skin tone wearing a random everyday head/body (the Create-A-Sim
    /// catalogue), with the adult wardrobe, age 27, no job; the character file and neighbour record are updated and
    /// the Sim is re-dressed on the adult skeleton in place (same object id).
    static bool ChildToAdult(VMAvatar* avatar, VMContext* context);
    /// Generic Sims Call 40 "pet to adult" (puppy/kitten grown by magic): age 27; the BHAV sets the scale (pd 85).
    static bool PetToAdult(VMAvatar* avatar);
    /// "dog" / "cat" for pets, "" for people.
    static std::string PetType(VMAvatar* avatar);
};

}}}}
