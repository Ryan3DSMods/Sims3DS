/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
// TS1(sims): The Sims 1 clothing collections and outfits (FreeSO TS1BCFProvider.CollectionsByName /
// TS1ClothingCollection, VMTS1PurchasableOutfitHelper, VMSuitProvider.GetPersonSuitTS1 - MPL-2.0, translated; FreeSims
// has none of it).
//
// Collections: every suit (appearance) name in the skins whose characters 1..3 are digits, grouped by its first letter
//   b body (daywear), c head, f formal (Hot Date), s swimwear, l pyjamas ("lounge"), w winter (Vacation), h high fashion
//   (Superstar), ...
// and by avatar type = the text between the number and '_' minus the skin tone: "mafat", "fcchd", "cat", "dog", ...
// (bodies are also listed under the type without the build: "ma" for b###mafat_...). Each suit is offered with the
// first texture that starts with "<suit up to '_'><skin>" (pets: "<suit up to '_'>"), as the game's wardrobe does.
//
// Person suits (Change Suit, person scope): 0 daywear (body string 1), 1 naked, 2 swimwear, 3 job uniform (career
// table), 4 formal, 5 sleepwear, 6/7 skeleton, 13/14/15/17/18 the bought formal / swimsuit / pyjamas / winter / high
// fashion outfits (body strings 30..34). Outfits a Sim does not own fall back to the base game's generic
// n/u/f/pajama meshes, like FreeSO.
#pragma once
#include <cstdint>
#include <functional>
#include <map>
#include <string>
#include <utility>
#include <vector>

namespace FSO { namespace Content { namespace TS1 {

class TS1Clothing {
public:
    static TS1Clothing& Get();
    /// Builds the collections from the avatar content (TS1AvatarContent); false without avatar data.
    bool EnsureBuilt();

    /// collection letter -> avatar type -> suit names (sorted)
    std::map<std::string, std::map<std::string, std::vector<std::string>>> Collections;
    const std::vector<std::string>* Suits(const std::string& collection, const std::string& avatarType) const;

    // ---- the purchasable outfits (Hot Date clothing racks, dressers, Unleashed pet coats) ----
    /// FreeSO OutfitTypes / OutfitTypeToInd: type 0 daywear "b" (body string 1), 1 formal "f" (30), 2 swimwear "s" (31),
    /// 3 pyjamas "l" (32), 4 winter "w" (33), 5 high fashion "h" (34).
    static const char* OutfitTypes[6];
    static const int16_t OutfitTypeToInd[6];

    struct Outfit { std::string BodyString; std::string HandTexSuffix; }; // "suit,BODY=tex", hand group suffix ("lgt", "_xxx")
    /// Outfits a Sim with these body strings can wear (`petType` "dog"/"cat" for pets, else from body string 1/14).
    /// outfitType < 0: pets without a Sim (-1 dog, -2 cat), as Make New Character uses.
    std::vector<Outfit> ValidOutfits(const std::vector<std::string>& bodyStrings, int16_t outfitType, const std::string& petType = "");
    /// Index of the Sim's current outfit of that type in ValidOutfits, -1 if not in the list.
    int16_t SuitIndex(const std::vector<std::string>& bodyStrings, int16_t outfitType, const std::string& petType = "");
    /// Sets outfit `newIndex` of that type into the body strings (and, for daywear, the hands' texture). Returns the
    /// previous index (-1), or -2 when newIndex is out of range (nothing changed).
    int16_t SetSuit(std::vector<std::string>& bodyStrings, int16_t outfitType, int16_t newIndex, const std::string& petType = "");

    /// A random head ("c" collection) and body ("b") for a new Sim of this kind (Make New Character):
    /// gender 'm'/'f', child, skin "lgt"/"med"/"drk". Returns false when the skins have none.
    bool RandomHeadBody(char gender, bool child, const std::string& skin, const std::function<int(int)>& rand,
                        std::string& head, std::string& body, std::string& handSuffix);

    /// Texture names (lower case, no extension) of the avatar content, sorted.
    const std::vector<std::string>& TextureNames();

private:
    bool _Built = false;
    std::vector<std::string> _Textures;
    std::string FirstTexture(const std::string& prefix);
};

/// FreeSO VMSuitProvider.GetPersonSuitTS1: the "mesh,BODY=texture" body string of person suit `id` for a Sim with these
/// body strings, or "" when the Sim has none (the caller keeps the current outfit). `jobMesh*` come from the career
/// level (CARR MaleUniformMesh / FemaleUniformMesh / UniformSkin), empty when unemployed.
std::string TS1PersonSuitBodyString(const std::vector<std::string>& bodyStrings, uint16_t id, bool male, bool child,
                                    const std::string& jobMaleMesh = "", const std::string& jobFemaleMesh = "",
                                    const std::string& jobSkin = "");

}}}
