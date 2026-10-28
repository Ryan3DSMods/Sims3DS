/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
// FSO.SimAntics.Engine.VMDialogHandler
#include "simantics/engine/VMDialogHandler.h"
#include "simantics/engine/VMStackFrame.h"
#include "simantics/engine/VMMemory.h"
#include "simantics/entities/VMEntity.h"
#include "simantics/entities/VMAvatar.h"
#include "simantics/model/VMPersonDataVariable.h"
#include "simantics/model/VMDialogInfo.h"
#include "simantics/VM.h"
#include "simantics/VMContext.h"
#include <cctype>
#include <cstdio>
#include "content/ContentManager.h"
#include "content/TS1JobProvider.h"
#include "content/TS1NeighbourProvider.h"
#include "simantics/entities/VMMultitileGroup.h"
#include "files/formats/iff/chunks/CTSS.h"
#include "files/formats/iff/chunks/OBJD.h"
#include <stdexcept>
#include <algorithm>
namespace FSO { namespace SimAntics { namespace Engine {
using namespace FSO::Files::Formats::IFF::Chunks;
using namespace FSO::SimAntics::Primitives;
using Scopes::VMVariableScope;
using Utils::VMMemory;

// FIX(sims): FreeSO's token set (TS1 strings use $Job:/$JobDesc:/$JobOffer:/$TimeLocal:/$Neighbor/... ; FreeSims only knew 13,
// so e.g. the promotion letter read "You have been promoted! (to Job:4:5) JobDesc:4:5 You now work from TimeLoc...").
const char* const VMDialogHandler::valid[26] = {
    "Object", "Me", "TempXL:", "Temp:", "$", "Attribute:", "DynamicStringLocal:", "Local:", "TimeLocal:", "NameLocal:",
    "FixedLocal:", "DynamicObjectName", "MoneyXL:", "JobOffer:", "Job:", "JobDesc:", "Param:", "Neighbor", "\r\n", "ListObject",
    "CatalogLocal:", "DateLocal:", "ObjectLocal:", "\\n",
    "TokenNameLocal:", // FIX(sims): TS1 expansion token menus (not in FreeSO)
    "NeighborLocal:"   // FIX(sims) QA3: Superstar 'CT - Promotion Dialog' ("Congratulations $NeighborLocal:2!")
};

// STR.GetString returns null for missing entries in C#; mirror that with TryGetString.
static bool StrGet(STR* s, int32_t index, std::string& out) {
    if (s == nullptr) return false;
    return s->TryGetString(index, STR::DefaultLangCode, out);
}

void VMDialogHandler::ShowDialog(VMStackFrame* context, VMDialogOperand* operand, STR* source) {
    Model::VMDialogInfo info;
    std::string s;
    info.Block = (operand->Flags & VMDialogFlags::Continue) == 0;
    info.Caller = context->Caller;
    info.Icon = context->StackObject;
    info.Operand = *operand;
    { bool ok = StrGet(source, std::max(0, operand->MessageStringID - 1), s); info.Message = ParseDialogString(context, s, source, ok); }
    if (operand->TitleStringID == 0) info.Title = ""; else { bool ok = StrGet(source, operand->TitleStringID - 1, s); info.Title = ParseDialogString(context, s, source, ok); }
    if (operand->IconNameStringID == 0) info.IconName = ""; else { bool ok = StrGet(source, operand->IconNameStringID - 1, s); info.IconName = ParseDialogString(context, s, source, ok); }
    if (operand->YesStringID != 0) { bool ok = StrGet(source, operand->YesStringID - 1, s); info.Yes = ParseDialogString(context, s, source, ok); info.HasYes = true; }
    if (operand->NoStringID != 0) { bool ok = StrGet(source, operand->NoStringID - 1, s); info.No = ParseDialogString(context, s, source, ok); info.HasNo = true; }
    if (operand->CancelStringID != 0) { bool ok = StrGet(source, operand->CancelStringID - 1, s); info.Cancel = ParseDialogString(context, s, source, ok); info.HasCancel = true; }
    info.DialogID = (context->CodeOwner->GUID << 32) | ((uint64_t)context->Routine->ID << 16) | context->InstructionPointer;
    context->VM()->SignalDialog(info);
}

bool VMDialogHandler::CommandSubstrValid(const std::string& command) {
    for (int i = 0; i < 26; i++) {
        std::string v = valid[i];
        if (command.size() <= v.size() && command == v.substr(0, command.size())) return true;
    }
    return false;
}

namespace {
struct FormatException : std::runtime_error { FormatException() : std::runtime_error("Input string was not in a correct format.") {} };
// C# short.Parse: FormatException on empty/invalid, OverflowException (not caught by the parser) on overflow.
int16_t ShortParse(const std::string& num) {
    if (num.empty()) throw FormatException();
    long v = std::stol(num);
    if (v > 32767 || v < -32768) throw std::overflow_error("Value was either too large or too small for an Int16.");
    return (int16_t)v;
}
char CharAt(const std::string& s, int i) {
    if (i < 0 || i >= (int)s.size()) throw std::out_of_range("Index was outside the bounds of the array.");
    return s[i];
}
// C# int.ToString("##,#0")
std::string GroupThousands(int32_t v) {
    bool neg = v < 0;
    std::string digits = std::to_string(neg ? -(int64_t)v : (int64_t)v);
    std::string out;
    int count = 0;
    for (int i = (int)digits.size() - 1; i >= 0; i--) {
        out.insert(out.begin(), digits[i]);
        if (++count % 3 == 0 && i > 0) out.insert(out.begin(), ',');
    }
    return neg ? "-" + out : out;
}
}

std::string VMDialogHandler::ParseDialogString(VMStackFrame* context, const std::string& input, STR* source, bool hasInput) {
    int state = 0;
    std::string command;
    std::string output;

    if (!hasInput) return "Missing String!!!";

    for (int i = 0; i < (int)input.size(); i++) {
        if (state == 0) {
            if (input[i] == '$') {
                state = 1; //start parsing string
                command.clear();
            } else {
                output += input[i];
            }
        } else {
            command += input[i];
            if (i == (int)input.size() - 1 || !CommandSubstrValid(command)) {
                if (i != (int)input.size() - 1 || std::isdigit((unsigned char)input[i])) {
                    command.erase(command.size() - 1, 1);
                    i--;
                }

                auto cmdString = command;
                int16_t values[3] = { 0, 0, 0 };
                if (cmdString.size() > 1 && cmdString[cmdString.size() - 1] == ':') {
                    try {
                        if (cmdString == "DynamicStringLocal:" || cmdString == "TimeLocal:" || cmdString == "JobOffer:" || cmdString == "Job:" || cmdString == "JobDesc:" || cmdString == "DateLocal:") { // FIX(sims): FreeSO
                            values[1] = -1;
                            values[2] = -1;
                            for (int j = 0; j < 3; j++) {
                                char next = CharAt(input, ++i);
                                std::string num = "";
                                while (std::isdigit((unsigned char)next)) {
                                    num += next;
                                    next = (++i == (int)input.size()) ? '!' : input[i];
                                }
                                if (num == "") {
                                    values[j] = -1;
                                    if (j == 1) values[2] = -1;
                                    break;
                                }
                                values[j] = ShortParse(num);
                                if (i == (int)input.size() || next != ':') break;
                            }
                        } else {
                            char next = CharAt(input, ++i);
                            std::string num = "";
                            while (std::isdigit((unsigned char)next)) {
                                num += next;
                                next = (++i == (int)input.size()) ? '!' : input[i];
                            }
                            values[0] = ShortParse(num);
                        }
                        i--;
                    } catch (FormatException&) {
                    }
                }
                try {
                    if (cmdString == "Object" || cmdString == "DynamicObjectName") {
                        if (context->StackObject == nullptr) throw std::runtime_error("null");
                        // FIX(sims): FreeSO: the group name, else the catalogue name (CTSS 0), else the label
                        auto stackObj = context->StackObject;
                        std::string nm;
                        if (stackObj->MultitileGroup != nullptr && stackObj->MultitileGroup->Name != "") nm = stackObj->MultitileGroup->Name;
                        else {
                            auto obj = stackObj->MasterDefinition ? stackObj->MasterDefinition : stackObj->Object->OBJ;
                            auto strings = stackObj->Object->Resource->Get<FSO::Files::Formats::IFF::Chunks::CTSS>(obj->CatalogStringsID);
                            if (strings != nullptr && strings->Length() > 0) nm = strings->GetString(0);
                            if (nm.empty()) nm = stackObj->ToString();
                        }
                        output += nm;
                    } else if (cmdString == "Me") {
                        output += context->Caller->ToString();
                    } else if (cmdString == "TempXL:") {
                        output += std::to_string(VMMemory::GetBigVariable(context, VMVariableScope::TempXL, values[0]));
                    } else if (cmdString == "MoneyXL:") {
                        output += "$" + GroupThousands(VMMemory::GetBigVariable(context, VMVariableScope::TempXL, values[0]));
                    } else if (cmdString == "Temp:") {
                        output += std::to_string(VMMemory::GetBigVariable(context, VMVariableScope::Temps, values[0]));
                    } else if (cmdString == "$") {
                        output += "$"; i--;
                    } else if (cmdString == "Attribute:") {
                        output += std::to_string(VMMemory::GetBigVariable(context, VMVariableScope::MyObjectAttributes, values[0]));
                    } else if (cmdString == "DynamicStringLocal:") {
                        STR* res = nullptr;
                        bool skip = false;
                        if (values[2] != -1 && values[1] != -1) {
                            VMEntity* obj = context->VM()->GetObjectById((int16_t)context->Locals.at(values[2]));
                            if (obj == nullptr) skip = true;
                            else {
                                uint16_t tableID = (uint16_t)context->Locals.at(values[1]);
                                {//local
                                    if (obj->SemiGlobal != nullptr) res = obj->SemiGlobal->Get<STR>(tableID);
                                    if (res == nullptr) res = obj->Object->Resource->Get<STR>(tableID);
                                    if (res == nullptr) res = context->Global()->Resource->Get<STR>(tableID);
                                }
                            }
                        } else if (values[1] != -1) {
                            //global table
                            uint16_t tableID = (uint16_t)context->Locals.at(values[1]);
                            res = context->Global()->Resource->Get<STR>(tableID);
                        } else {
                            res = source;
                        }

                        if (!skip) {
                            uint16_t index = (uint16_t)context->Locals.at(values[0]);
                            if (res != nullptr) {
                                std::string str;
                                bool ok = StrGet(res, index, str);
                                output += ParseDialogString(context, str, res, ok); // recursive command parsing!
                                // this is needed for the crafting table.
                                // though it is also, completely insane?
                            }
                        }
                    } else if (cmdString == "Local:") {
                        output += std::to_string(VMMemory::GetBigVariable(context, VMVariableScope::Local, values[0]));
                    } else if (cmdString == "Param:") {
                        output += std::to_string(VMMemory::GetBigVariable(context, VMVariableScope::Parameters, values[0]));
                    } else if (cmdString == "NameLocal:") { // FIX(sims): FreeSO (was the literal "(NameLocal)")
                        auto o = context->VM()->GetObjectById(VMMemory::GetVariable(context, VMVariableScope::Local, values[0]));
                        if (o == nullptr) throw std::runtime_error("null");
                        output += o->ToString();
                    } else if (cmdString == "FixedLocal:") { // FIX(sims): FreeSO tokens from here on
                        char b[32]; snprintf(b, sizeof b, "%.2f", VMMemory::GetBigVariable(context, VMVariableScope::Local, values[0]) / 100.0f);
                        output += b;
                    } else if (cmdString == "TimeLocal:") {
                        int32_t hours = VMMemory::GetBigVariable(context, VMVariableScope::Local, values[0]);
                        int32_t mins = (values[1] == -1) ? 0 : VMMemory::GetBigVariable(context, VMVariableScope::Local, values[1]);
                        const char* suffix = (hours > 11) ? "pm" : "am";
                        if (hours > 12) hours -= 12;
                        char b[32]; snprintf(b, sizeof b, "%d:%02d%s", hours, mins, suffix);
                        output += b;
                    } else if (cmdString == "ObjectLocal:") {
                        auto o = context->VM()->GetObjectById(VMMemory::GetVariable(context, VMVariableScope::Local, values[0]));
                        if (o != nullptr) output += o->ToString();
                    } else if (cmdString == "JobOffer:") {
                        auto c = FSO::Content::Content::Get();
                        if (c && c->Jobs) output += c->Jobs->JobOffer((int16_t)VMMemory::GetBigVariable(context, VMVariableScope::Local, values[0]),
                                                                       VMMemory::GetBigVariable(context, VMVariableScope::Local, values[1]));
                    } else if (cmdString == "Job:" || cmdString == "JobDesc:") {
                        int32_t level = VMMemory::GetBigVariable(context, VMVariableScope::Local, values[1]);
                        auto c = FSO::Content::Content::Get();
                        auto jobStr = (c && c->Jobs) ? c->Jobs->JobStrings((int16_t)VMMemory::GetBigVariable(context, VMVariableScope::Local, values[0])) : nullptr;
                        std::string js;
                        if (jobStr != nullptr && StrGet(jobStr, level * 3 + ((cmdString == "JobDesc:") ? 3 : 4), js)) output += js;
                    } else if (cmdString == "Neighbor" || cmdString == "NeighborLocal:") {
                        auto c = FSO::Content::Content::Get();
                        std::string nm = "Unknown";
                        if (c && c->Neighborhood) {
                            // FIX(sims) QA3: "$NeighborLocal:N" = the neighbour whose id is in local N
                            int16_t nid = (cmdString == "Neighbor") ? context->StackObjectID()
                                : (int16_t)VMMemory::GetBigVariable(context, VMVariableScope::Local, values[0]);
                            auto n = c->Neighborhood->GetNeighborByID(nid);
                            uint32_t guid = n ? n->GUID : 0;
                            auto gobj = guid ? c->WorldObjects->Get(guid, true) : nullptr;
                            if (gobj != nullptr) {
                                auto ctss = gobj->Resource->Get<FSO::Files::Formats::IFF::Chunks::CTSS>(gobj->OBJ->CatalogStringsID);
                                if (ctss != nullptr && ctss->Length() > 0) nm = ctss->GetString(0);
                            }
                        }
                        output += nm;
                    } else if (cmdString == "ListObject") {
                        if (context->StackObject != nullptr) for (auto x : context->StackObject->MyList) output += (char)x;
                    } else if (cmdString == "CatalogLocal:") {
                        auto catObj = context->VM()->GetObjectById(VMMemory::GetVariable(context, VMVariableScope::Local, values[0]));
                        if (catObj != nullptr) {
                            auto ctss = catObj->Object->Resource->Get<FSO::Files::Formats::IFF::Chunks::CTSS>(catObj->Object->OBJ->CatalogStringsID);
                            if (ctss != nullptr && ctss->Length() > 1) output += ctss->GetString(1);
                        }
                    } else if (cmdString == "DateLocal:") {
                        static const char* months[] = { "January", "February", "March", "April", "May", "June", "July", "August", "September", "October", "November", "December" };
                        int d = context->Locals.at(values[0]), m = context->Locals.at(values[1]), y = context->Locals.at(values[2]);
                        char b[64]; snprintf(b, sizeof b, "%s %d, %d", (m >= 1 && m <= 12) ? months[m - 1] : "?", d, y);
                        output += b;
                    } else if (cmdString == "TokenNameLocal:") {
                        // FIX(sims): "$TokenNameLocal:N" (Makin' Magic EverAfter Crafter / cauldron 'Add...' menus): the
                        // catalogue name of the caller's inventory token at the index in local N (Manage Inventory 'find
                        // token' writes that index). FreeSO lacks the token, so the menu read "TokenNameLocal:1".
                        auto c = FSO::Content::Content::Get();
                        auto av = dynamic_cast<VMAvatar*>(context->Caller);
                        auto inv = (c && c->Neighborhood && av) ? c->Neighborhood->GetInventoryByNID(av->GetPersonData(Model::VMPersonDataVariable::NeighborId)) : nullptr;
                        int idx = VMMemory::GetVariable(context, VMVariableScope::Local, values[0]);
                        if (inv != nullptr && idx >= 0 && idx < (int)inv->size()) {
                            auto gobj = c->WorldObjects->Get((*inv)[idx].GUID, true);
                            std::string nm;
                            if (gobj != nullptr) {
                                auto ctss = gobj->Resource->Get<FSO::Files::Formats::IFF::Chunks::CTSS>(gobj->OBJ->CatalogStringsID);
                                if (ctss != nullptr && ctss->Length() > 0) nm = ctss->GetString(0);
                                if (nm.empty()) nm = gobj->OBJ->ChunkLabel;
                            }
                            output += nm;
                        }
                    } else if (cmdString == "\\n") {
                        output += "\n";
                    } else if (cmdString.empty()) {
                        // FIX(sims) QA2: a '$' that starts no token is a literal (pie "Call Cab.../Go Downtown - $50" read
                        // "Go Downtown - 50"; "Go On Vacation - 500")
                        output += "$";
                    } else {
                        output += cmdString;
                    }
                } catch (std::exception&) {
                    //something went wrong. just skip command
                }
                state = 0;
            }
        }
    }
    // output.Replace("\r\n", "\r\n\r\n");
    std::string result;
    for (size_t k = 0; k < output.size(); k++) {
        if (output[k] == '\r' && k + 1 < output.size() && output[k + 1] == '\n') { result += "\r\n\r\n"; k++; }
        else result += output[k];
    }
    return result;
}
}}}
