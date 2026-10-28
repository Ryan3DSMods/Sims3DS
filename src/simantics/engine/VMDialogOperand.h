/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
// FSO.SimAntics.Primitives.VMDialogOperand / VMDialogFlags / VMDialogType / VMDialogResult
// (primitives/VMDialogPrivateStrings.cs). Owned by the core because VMDialogHandler/VMThread/
// VMDialogInfo need them; primitives/VMDialog*Strings.* must include this.
#pragma once
#include <cstdint>
#include <string>
#include "simantics/_files_contract.h"
#include "simantics/engine/VMPrimitiveOperand.h"
#include "simantics/model/TSOPlatform/VMAsyncState.h"
namespace FSO { namespace SimAntics { namespace Primitives {

enum class VMDialogFlags : int32_t {
    Continue = 1,
    Unknown1 = 2, // icon type? 3 bit int
    Unknown2 = 4, //
    Unknown3 = 8, // always 0 in tso.
    // Icon type: 0 = auto, 1 = none, 2 = neighbour, 3 = indexed, 4 = named
    UseTempXL = 16,
    UseTemp1 = 32,
    FilterProfanity = 64,
    NewEngageContinue = 128
};
FSO_FLAGS_ENUM(VMDialogFlags)

enum class VMDialogType : uint8_t {
    Message = 0, YesNo = 1, YesNoCancel = 2, TextEntry = 3, Sims1Tutorial = 4, NumericEntry = 5,
    ImageMapped = 6, //truncated in edith..
    Custom = 7, UserBitmap = 8,

    // TS1(sims): FreeSO's TS1 dialog types (Behavior.iff STR#217 'dialog types'): lot/selection screens the
    // expansion packs open. The lot selection ones return the chosen house number in temp 0 (ResponseText); the map
    // to show is VMTS1Travel::LotMapForDialog (a TS1NeighborhoodProvider::LotMaps id).
    TS1Downtown = 5, //house number in temp0
    TS1Clothes = 6,
    TS1Vacation = 7,
    TS1Neighborhood = 8,
    TS1PetChoice = 9,
    TS1PhoneBook = 10,
    TS1StudioTown = 11,
    TS1Spellbook = 12,
    TS1Magictown = 13,
    TS1TransformMe = 14,
    TS1Cookbook = 15
};

class VMDialogOperand : public Engine::VMPrimitiveOperand {
public:
    //engage and block sim, automatic icon, local reference 0, string not debug
    uint8_t CancelStringID = 0; //button 3. renaming used for genie, as an example.
    uint8_t IconNameStringID = 0;
    uint8_t MessageStringID = 0;
    uint8_t YesStringID = 0; //button 1
    uint8_t NoStringID = 0; //button 2
    VMDialogType Type = VMDialogType::Message;
    uint8_t TitleStringID = 0;
    VMDialogFlags Flags = (VMDialogFlags)0;

    void Read(const uint8_t* bytes) override {
        st::Reader io(bytes, 8, st::Endian::Little);
        CancelStringID = io.u8();
        IconNameStringID = io.u8();
        MessageStringID = io.u8();
        YesStringID = io.u8();
        NoStringID = io.u8();
        Type = (VMDialogType)io.u8();
        TitleStringID = io.u8();
        Flags = (VMDialogFlags)io.u8();
    }
    void Write(uint8_t* bytes) override {
        bytes[0] = CancelStringID; bytes[1] = IconNameStringID; bytes[2] = MessageStringID; bytes[3] = YesStringID;
        bytes[4] = NoStringID; bytes[5] = (uint8_t)Type; bytes[6] = TitleStringID; bytes[7] = (uint8_t)Flags;
    }
};

class VMDialogResult : public Model::TSOPlatform::VMAsyncState {
public:
    int32_t Timeout = 30 * 60;
    uint8_t ResponseCode = 0; //0,1,2 = yes/ok,no,cancel.
    std::string ResponseText = "";
    //local variables
    bool HasDisplayed = false; //re-display dialog if we restore into it being up, in case UI missed it
    VMDialogType Type = VMDialogType::Message; //used for input sanitization
    Model::TSOPlatform::VMAsyncStateType StateType() const override { return Model::TSOPlatform::VMAsyncStateType::DialogResult; }
};
}}}
