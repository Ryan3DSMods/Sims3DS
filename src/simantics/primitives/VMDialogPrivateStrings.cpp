/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
// FSO.SimAntics.Primitives.VMDialogPrivateStrings (primitives/VMDialogPrivateStrings.cs)
#include "simantics/primitives/VMDialogPrivateStrings.h"
#include <cerrno>
#include <cctype>
#include <cstdlib>
#include <climits>
#include <memory>
#include <string>
#include "simantics/engine/VMStackFrame.h"
#include "simantics/engine/VMThread.h"
#include "simantics/engine/VMDialogHandler.h"
#include "simantics/engine/VMSimanticsException.h"
#include "simantics/entities/VMEntity.h"
#include "simantics/entities/VMAvatar.h"
#include "simantics/VM.h"
#include "simantics/VMContext.h"

namespace FSO { namespace SimAntics { namespace Primitives {

using namespace FSO::SimAntics::Engine;
using FSO::Files::Formats::IFF::Chunks::STR;

const int32_t VMDialogPrivateStrings::DIALOG_MAX_WAITTIME = 60 * 30;

namespace {
// C# int.TryParse(string, out int) (NumberStyles.Integer: optional surrounding whitespace, optional sign, digits).
bool TryParseInt32(const std::string& s, int32_t& result)
{
    result = 0;
    size_t b = 0, e = s.size();
    while (b < e && std::isspace((unsigned char)s[b])) b++;
    while (e > b && std::isspace((unsigned char)s[e - 1])) e--;
    if (b == e) return false;
    size_t i = b;
    if (s[i] == '+' || s[i] == '-') i++;
    if (i == e) return false;
    for (size_t j = i; j < e; j++) if (!std::isdigit((unsigned char)s[j])) return false;
    std::string body = s.substr(b, e - b);
    errno = 0;
    long long v = std::strtoll(body.c_str(), nullptr, 10);
    if (errno == ERANGE || v < INT32_MIN || v > INT32_MAX) return false;
    result = (int32_t)v;
    return true;
}
}

VMPrimitiveExitCode VMDialogPrivateStrings::Execute(VMStackFrame* context, VMPrimitiveOperand* args)
{
    return ExecuteGeneric(context, args, context->ScopeResource()->Get<STR>(301));
}

VMPrimitiveExitCode VMDialogPrivateStrings::ExecuteGeneric(VMStackFrame* context, VMPrimitiveOperand* args, STR* table)
{
    auto operand = static_cast<VMDialogOperand*>(args);
    // C#: (VMDialogResult)context.Thread.BlockingState — invalid cast throws.
    auto& blocking = context->Thread->BlockingState;
    if (blocking != nullptr && blocking->StateType() != Model::TSOPlatform::VMAsyncStateType::DialogResult)
        throw VMSimanticsException("Dialog: thread blocking state is not a VMDialogResult", context);
    auto curDialog = std::static_pointer_cast<VMDialogResult>(blocking);
    if (curDialog == nullptr)
    {
        VMDialogHandler::ShowDialog(context, operand, table);

        if ((operand->Flags & VMDialogFlags::Continue) == 0)
        {
            auto result = std::make_shared<VMDialogResult>();
            result->Type = operand->Type;
            result->HasDisplayed = true;
            context->Thread->BlockingState = result;
            return VMPrimitiveExitCode::CONTINUE_NEXT_TICK;
        }
        else return VMPrimitiveExitCode::GOTO_TRUE;
    }
    else
    {
        if (curDialog->Responded || curDialog->WaitTime > DIALOG_MAX_WAITTIME)
        {
            context->Thread->BlockingState = nullptr;
            context->VM()->SignalDialog(nullptr);
            switch (curDialog->Type)
            {
                default:
                case VMDialogType::Message:
                    return VMPrimitiveExitCode::GOTO_TRUE;
                case VMDialogType::YesNo:
                    return (curDialog->ResponseCode == 0) ? VMPrimitiveExitCode::GOTO_TRUE : VMPrimitiveExitCode::GOTO_FALSE;
                case VMDialogType::YesNoCancel:
                    if (curDialog->ResponseCode > 1)
                    {
                        context->Thread->TempRegisters[((operand->Flags & VMDialogFlags::UseTemp1) > 0) ? 1 : 0] = (int16_t)((curDialog->ResponseCode - 1) % 2);
                        return VMPrimitiveExitCode::GOTO_FALSE;
                    }
                    else return VMPrimitiveExitCode::GOTO_TRUE;
                case VMDialogType::TextEntry:
                    //todo: filter profanity, limit name length
                    //also verify behaviour.
                    if (curDialog->ResponseText != "")
                        static_cast<VMAvatar*>(context->StackObject)->Name(curDialog->ResponseText);
                    return VMPrimitiveExitCode::GOTO_TRUE;
                // TS1(sims): FreeSO — the TS1 lot selection / phone book dialogs answer a number in temp 0 like
                // NumericEntry ("also downtown": TS1Downtown == NumericEntry); pet choice / clothes treat code 0 as cancel.
                case VMDialogType::TS1PetChoice:
                case VMDialogType::TS1Clothes:
                    if (context->VM()->TS1 && curDialog->ResponseCode == 0) return VMPrimitiveExitCode::GOTO_FALSE;
                    // fall through
                case VMDialogType::TS1Vacation:
                case VMDialogType::TS1Neighborhood:
                case VMDialogType::TS1StudioTown:
                case VMDialogType::TS1Magictown:
                case VMDialogType::TS1PhoneBook:
                    if (!context->VM()->TS1) return VMPrimitiveExitCode::GOTO_TRUE; // TSO: Custom/UserBitmap/...
                    // fall through
                case VMDialogType::NumericEntry:
                {
                    int32_t number;
                    if (!TryParseInt32(curDialog->ResponseText, number)) return VMPrimitiveExitCode::GOTO_FALSE;

                    auto tempNumber = ((operand->Flags & VMDialogFlags::UseTemp1) > 0) ? 1 : 0;

                    if ((operand->Flags & VMDialogFlags::UseTempXL) > 0) context->Thread->TempXL[tempNumber] = number;
                    else context->Thread->TempRegisters[tempNumber] = (int16_t)number;
                    return VMPrimitiveExitCode::GOTO_TRUE;
                }
            }
        }
        else
        {
            if (!curDialog->HasDisplayed)
            {
                VMDialogHandler::ShowDialog(context, operand, table);
                curDialog->HasDisplayed = true;
            }
            return VMPrimitiveExitCode::CONTINUE_NEXT_TICK;
        }
    }
}

}}}
