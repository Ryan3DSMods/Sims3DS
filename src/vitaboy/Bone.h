/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
// FSO.Vitaboy.Bone (sims.common/rendering/vitaboy/Bone.cs)
// (PropertyListItem from Bone.cs is declared in Animation.h.)
#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include "vitaboy/VitaboyMath.h"
#include "vitaboy/Animation.h"

namespace FSO { namespace Vitaboy {

/// <summary>
/// Bones are used to animate characters. They hold rotation and translation data.
/// </summary>
class Bone {
public:
    int32_t Unknown = 0;
    std::string Name;
    std::string ParentName;

    uint8_t HasProps = 0;
    std::vector<PropertyListItem> Properties;

    Vector3 Translation;
    Quaternion Rotation;

    int32_t CanTranslate = 0;
    int32_t CanRotate = 0;
    int32_t CanBlend = 0;
    int32_t Index = 0;

    float WiggleValue = 0;
    float WigglePower = 0;

    std::vector<Bone*> Children; // non-owning; owned by the Skeleton

    //Dummy & debug
    Vector3 AbsolutePosition;
    Matrix AbsoluteMatrix;

    /// <summary>
    /// Clones this bone.
    /// </summary>
    Bone* Clone() const {
        auto result = new Bone();
        result->Unknown = Unknown;
        result->Name = Name;
        result->ParentName = ParentName;
        result->HasProps = HasProps;
        result->Properties = Properties;
        result->Translation = Translation;
        result->Rotation = Rotation;
        result->CanTranslate = CanTranslate;
        result->CanRotate = CanRotate;
        result->CanBlend = CanBlend;
        result->WiggleValue = WiggleValue;
        result->WigglePower = WigglePower;
        result->Index = Index;
        return result;
    }
};

}}
