/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
// Translated from FreeSims SimsVille/SimsAntics/primitives/VMFindBestAction.cs
#include "simantics/primitives/VMFindBestAction.h"
#include <algorithm>
#include <cmath>
#include <unordered_set>
#include <vector>
#include "simantics/engine/VMStackFrame.h"
#include "simantics/engine/VMSimanticsException.h"
#include "simantics/entities/VMEntity.h"
#include "simantics/entities/VMAvatar.h"
#include "simantics/entities/VMMultitileGroup.h"
#include "simantics/model/VMMotive.h"
#include "simantics/VM.h"
#include "simantics/VMContext.h"
#include "files/formats/iff/chunks/TTAB.h"
#include "core/io.h"
#include "simantics/engine/VMThread.h"
#include "simantics/engine/VMQueuedAction.h"
#include "simantics/entities/VMGameObject.h"
#include "simantics/model/VMPersonDataVariable.h"
#include "simantics/model/VMStackObjectVariable.h"
#include "content/ContentManager.h"

namespace FSO { namespace SimAntics { namespace Primitives {
using namespace FSO::SimAntics::Engine;
using namespace FSO::SimAntics::Model;
using FSO::Files::Formats::IFF::Chunks::TTAB;
using FSO::LotView::Model::LotTilePos;

// ===================================================================================================================
// TS1(sims): The Sims 1 autonomy, translated from FreeSO tso.simantics/Primitives/VMFindBestAction.cs
// (FreeSims' version below is a TSO-style placeholder). FreeSO's notes:
//
// How autonomy works in The Sims:
//
// Each interaction has "motive advertisements" which determine how many autonomy points are given for each interaction.
//
// First, each motive is converted into an "effective motive" using the interaction contribution curves.
// These curves generally make motive changes more evident at lower values, and cap them at a specific value.
// This prevents people from considering sleeping at 50% energy, but also allows other motives like fun to use a much higher cap.
// "Happy" is calculated as an average of all "effective motive"s, including mood.
// (In Hot Date and onwards this may be weighted by the Happy Weight curves)
//
// For each interaction, we only evaluate it if it has the following properties:
// - non-zero motive range for any advertisement
// - object is not "occupied"
// - object has use count of 0 (or the interaction is joinable)
// - if any advertisements have a non-zero minimum, they are only considered if the motive is below that minimum
// Interaction check trees are evaluated to see if they are usable. "Auto" is passed to the tree as 1 in param 0, indicating this is an autonomous check.
// When evaluating an interaction's check tree, it may change the motive ad range and minimum.
//
// For each interaction a new Happy score is calculated based on the motive deltas applied to the sim's current motives.
// Motive delta is divided by 1000 for some reason. It matches numbers with TS 1.0.
// Personality is applied by **some factor** based on your personality, if present. Likely 0-1 multiplier for personality, 0-2 for skill.
// Remember to re-apply the interaction curves to the new motives here.
//
// Attenuation is very simple: Score / (1 + (Attenuation* Distance)). It is applied to the *distance from base happy*, which results in the final score.
// Individual motive scores can be negative to discourage use of interactions at high motives.These are combined with positives from other motives.
//
// Final scores below certain values are ignored:
// 1E-07 for visitors and family, 1E-06 for sims who are sitting. These constants are in FCNS 2 in global.iff.
// That means negative scores are ignored. Motive ads when you're in the flat part of the curve are ignored.
//
// sims additions (marked below): objects whose "lockout count" (object data 25) is non-zero are skipped — see
// STUBS_autonomy.md; FreeSO left this as a TODO.
// ===================================================================================================================

const VMPersonDataVariable VMFindBestAction::VaryByTypes[23] = {
    VMPersonDataVariable::UnusedAndDoNotUse, //0
    VMPersonDataVariable::NicePersonality,
    VMPersonDataVariable::NicePersonality, //inverted
    VMPersonDataVariable::ActivePersonality,
    VMPersonDataVariable::ActivePersonality, //inverted
    VMPersonDataVariable::GenerousPersonality,
    VMPersonDataVariable::GenerousPersonality, //inverted
    VMPersonDataVariable::PlayfulPersonality,
    VMPersonDataVariable::PlayfulPersonality, //inverted
    VMPersonDataVariable::OutgoingPersonality,
    VMPersonDataVariable::OutgoingPersonality, //inverted
    VMPersonDataVariable::NeatPersonality,
    VMPersonDataVariable::NeatPersonality, //inverted

    VMPersonDataVariable::UnusedAndDoNotUse, // cleaning skill -- 13
    VMPersonDataVariable::CookingSkill, // cooking skill
    VMPersonDataVariable::CharismaSkill, // social skill *
    VMPersonDataVariable::MechanicalSkill, // repair skill *
    VMPersonDataVariable::UnusedAndDoNotUse, // gardening skill
    VMPersonDataVariable::UnusedAndDoNotUse, // music skill
    VMPersonDataVariable::CreativitySkill, // creativity skill
    VMPersonDataVariable::UnusedAndDoNotUse, // literacy skill
    VMPersonDataVariable::BodySkill, // physical skill *
    VMPersonDataVariable::LogicSkill // logic skill
};

const VMMotive VMFindBestAction::WeightMotives[9] = {
    VMMotive::Energy,
    VMMotive::Comfort,
    VMMotive::Hunger,
    VMMotive::Hygiene,
    VMMotive::Bladder,
    VMMotive::Mood,
    VMMotive::Room,
    VMMotive::Social,
    VMMotive::Fun
};

std::function<void(VMEntity*, VMEntity*, const std::string&, bool, float)> VMFindBestAction::DebugCandidate;

int VMFindBestAction::MotiveToWeight(int motive) {
    for (int i = 0; i < 9; i++) if ((int)WeightMotives[i] == motive) return i;
    return -1;
}

std::vector<VMPieMenuInteraction> VMFindBestAction::TakeTopActions(const std::vector<VMPieMenuInteraction>& list, int count) {
    std::vector<VMPieMenuInteraction> result;
    for (auto& action : list) {
        auto users = action.Callee->GetValue(VMStackObjectVariable::UseCount);
        bool joinable = false;
        if (action.Callee->TreeTable != nullptr)
            for (auto& x : action.Callee->TreeTable->Interactions) if (x->JoiningIndex == (int32_t)action.ID) { joinable = true; break; }
        if (users == 0 || joinable) {
            result.push_back(action);
            if ((int)result.size() >= count) break;
        }
    }
    return result;
}

VMPrimitiveExitCode VMFindBestAction::ExecuteTS1(VMStackFrame* context) {
    auto callerAvatar = dynamic_cast<VMAvatar*>(context->Caller);
    if (callerAvatar == nullptr) throw VMSimanticsException("Find Best Action: caller is not an avatar (invalid cast).", context);
    auto thread = context->Caller->Thread.get();

    //if we already have some action, do nothing
    auto myPriority = callerAvatar->GetPersonData(VMPersonDataVariable::Priority);
    for (auto& x : thread->Queue) {
        if (x->Priority > myPriority) {
            context->StackObject = x->Callee;
            return VMPrimitiveExitCode::GOTO_TRUE;
        }
    }

    auto ents = std::vector<VMEntity*>(context->VM()->Entities); // C#: ObjectQueries.WithAutonomy (objects with auto interactions)
    std::unordered_set<int16_t> processed;
    auto caller = callerAvatar;
    auto pos1 = caller->Position();

    auto visitor = (caller->GetPersonData(VMPersonDataVariable::PersonType) == 1);
    auto child = (caller->GetPersonData(VMPersonDataVariable::PersonsAge) < 18 && context->VM()->TS1); // C# caller.IsChild
    auto attenTable = visitor ? TTAB::VisitorAttenuationValues : TTAB::TS1AttenuationValues;
    auto global = FSO::Content::Content::Get()->WorldObjectGlobals.get();
    global->InitCurves();
    auto& interactionCurve = child ? global->InteractionScoreChild : global->InteractionScore;
    if (interactionCurve.size() < 9) return VMPrimitiveExitCode::GOTO_FALSE; // no Global.iff curves

    auto isStray = caller->IsPet() && context->VM()->TS1 && caller->GetPersonData(VMPersonDataVariable::GreetStatus) == 0 && caller->GetPersonData(VMPersonDataVariable::PersonType) == 1;

    // === HAPPY CALCULATION ===

    auto newStyle = false;
    // TODO: new style
    float weights[9] = { 1, 1, 1, 1, 1, 1, 1, 1, 1 }; //TODO: weights from curves for new
    (void)newStyle;
    float totalWeight = 0;
    for (int i = 0; i < 9; i++) totalWeight += weights[i];
    for (int i = 0; i < 9; i++) weights[i] /= totalWeight;
    double minScore = (caller->GetPersonData(VMPersonDataVariable::Posture) > 0) ? 1e-6 : 1e-7;

    float happyParts[9];
    float baseHappy = 0;
    for (int i = 0; i < 9; i++) {
        happyParts[i] = interactionCurve[i].GetPoint(caller->GetMotiveData(WeightMotives[i])) * weights[i];
        baseHappy += happyParts[i];
    }

    std::vector<VMPieMenuInteraction> validActions;
    for (auto iobj : ents) {
        if (iobj->Dead || iobj->Position() == LotTilePos::OUT_OF_WORLD) continue;
        auto obj = iobj->MultitileGroup->GetInteractionGroupLeader(iobj);
        auto gobj = dynamic_cast<VMGameObject*>(obj);
        if (processed.count(obj->ObjectID) || (gobj != nullptr && (int)gobj->Disabled > 0)) continue;
        processed.insert(obj->ObjectID);

        // TS1(sims): lockout count. The Sims 1 trees set the used/failed object's "lockout count" (object data 25,
        // e.g. global 'Standard Exit' := param, 'fail food chain object' := 200) and the engine counts it down every
        // tick (VMEntity::Tick); FreeSO's Find Best Object For Function skips locked out objects, its Find Best Action
        // has it as a TODO. Without it Sims re-choose the interaction they just finished or failed.
        if (obj->GetValue(VMStackObjectVariable::LockoutCount) > 0) continue;

        if (isStray) {
            if (gobj != nullptr) continue;
            //determine if the object is indoors
            auto roomID = context->VM()->Context->GetObjectRoom(obj);
            roomID = (uint16_t)std::max(0, std::min((int)context->VM()->Context->RoomInfo.size() - 1, (int)roomID));
            auto& room = context->VM()->Context->RoomInfo[roomID];
            auto outside = room.Room.IsOutside;
            if (!outside) continue;
        }
        auto pos2 = obj->Position();
        auto distance = (float)std::sqrt(std::pow(pos1.x - pos2.x, 2) + std::pow(pos1.y - pos2.y, 2) + std::pow((pos1.Level - pos2.Level) * 320, 2.0)) / 16.0f;
        auto inUse = obj->GetFlag(VMEntityFlags::Occupied);

        if (obj->TreeTable == nullptr) continue;
        for (auto entry : obj->TreeTable->AutoInteractions) {
            auto id = entry->TTAIndex;
            if (inUse) {
                bool joinable = false;
                for (auto& x : obj->TreeTable->Interactions) if (x->JoiningIndex == (int32_t)id) { joinable = true; break; }
                if (!joinable) continue;
            }
            auto& advertisements = entry->ActiveMotiveEntries;
            if (advertisements.size() == 0) continue; //no ads on this object.
            // FIX(sims) QA3: the TTAB autonomy threshold (Edith 'autonomy', 50 on 5400 entries, 100 on 3700) is never
            // reached by free will in TS1. CatGlobals disables the dog interactions it copied (Play With.../Fetch,
            // Wrestle, Dig, Sniff, Howl, Wriggle) only through it ('hide menu' test, threshold 50 -> 100), and the
            // base game puts 100 on what PC Sims never pick by themselves (Dismiss, Drain, Scold Pet, Kiss.../Suave,
            // Back Rub, Open starter kit ...). FreeSO ignores it, so Ginia played 'Fetch' with the cat Maggie and both
            // waited forever (the cat's 'Dog - Play - Fetch - Person B' waits for the dog socials object).
            if (entry->AutonomyThreshold >= 100) continue;

            auto action = obj->GetAction((int32_t)id, caller, context->VM()->Context);
            auto ttas = obj->TreeTableStrings;

            caller->ObjectData[(int)VMStackObjectVariable::HideInteraction] = 0;
            auto actionStrings = caller->Thread->CheckAction(action, true);

            std::vector<VMPieMenuInteraction> pie;
            if (actionStrings != nullptr) {
                if (actionStrings->size() > 0) {
                    for (auto& actionS : *actionStrings) {
                        if (actionS.Name.empty()) actionS.Name = (ttas != nullptr) ? ttas->GetString((int32_t)id) : std::string("***MISSING***");
                        actionS.ID = (uint8_t)id;
                        actionS.Entry = entry;
                        actionS.Global = false;
                        pie.push_back(actionS);
                    }
                } else {
                    if (ttas != nullptr) {
                        VMPieMenuInteraction item;
                        item.Name = ttas->GetString((int32_t)id);
                        item.ID = (uint8_t)id;
                        item.Entry = entry;
                        item.Global = false;
                        pie.push_back(item);
                    }
                }
            }
            if (pie.size() == 0 && DebugCandidate) DebugCandidate(caller, obj, (ttas != nullptr) ? ttas->GetString((int32_t)id) : std::string("?"), false, 0);
            if (pie.size() > 0) {
                auto& first = pie[0];
                // calculate score for this tree.
                // start with the base happy value, and modify it for each motive changed.
                float score = baseHappy;
                for (size_t i = 0; i < advertisements.size(); i++) {
                    auto motiveI = advertisements[i].MotiveIndex;
                    auto& motiveScore = entry->MotiveEntries[motiveI];
                    int16_t min = motiveScore.EffectRangeMinimum;
                    int16_t max = motiveScore.EffectRangeMaximum; // FreeSO: EffectRangeDelta
                    int16_t personality = (int16_t)motiveScore.PersonalityModifier;
                    if (first.MotiveAdChanges != nullptr) {
                        // FIX(sims): C# TryGetValue(key, out min) zeroes the value when the key is missing, so a
                        // check tree that changed one ad wiped all the others. Only replace what the tree set.
                        auto& ads = *first.MotiveAdChanges;
                        auto f = ads.find((0 << 16) | motiveI); if (f != ads.end()) min = f->second;
                        f = ads.find((1 << 16) | motiveI); if (f != ads.end()) max = f->second;
                        f = ads.find((2 << 16) | motiveI); if (f != ads.end()) personality = f->second;
                    }

                    if (max == 0 && min > 0) {
                        //invalid delta. do from 0..delta instead (fix child-talk preference?)
                        max = min;
                        min = 0;
                    }
                    max += min; //it's a delta, add min to it

                    auto myMotive = caller->GetMotiveData((VMMotive)motiveI);
                    if (min != 0 && myMotive > min) continue;

                    // subtract the base contribution for this motive from happy
                    auto weightInd = MotiveToWeight(motiveI);
                    if (weightInd == -1) continue;
                    score -= happyParts[weightInd];

                    float personalityMul = 1;
                    if (personality > 0 && personality < 23) {
                        personalityMul = caller->GetPersonData(VaryByTypes[personality]);
                        personalityMul /= 1000.0f;
                        if (personality < 13) {
                            if ((personality & 1) == 0) {
                                personalityMul = 1 - personalityMul;
                            }
                        } else {
                            personalityMul *= 2;
                        }
                    }

                    // then add the new contribution for this motive.
                    score += interactionCurve[weightInd].GetPoint(myMotive + (max * personalityMul) / 1000.0f) * weights[weightInd];
                }

                // score relative to base
                score -= baseHappy;
                // modify score using attenuation
                float atten = (entry->AttenuationCode == 0 || entry->AttenuationCode >= 5) ?
                    entry->AttenuationValue : attenTable[entry->AttenuationCode];

                score = score / (1 + atten * distance);
                if (DebugCandidate) DebugCandidate(caller, obj, first.Name, true, score);

                if (score > minScore) {
                    for (auto& item : pie) {
                        item.Score = score;
                        item.Callee = obj;
                        // FIX(sims): C# adds `first` once per action string (so only the first sub-option of a
                        // social could ever be chosen); add each string, carrying its own name/param.
                        validActions.push_back(item);
                    }
                }
            }
            //TODO: Lockout interactions that have been used before for a few sim hours (in ts1 ticks. same # of ticks for tso probably)
            //TODO: special logic for socials?
        }
    }
    std::stable_sort(validActions.begin(), validActions.end(), [](const VMPieMenuInteraction& a, const VMPieMenuInteraction& b) { return a.Score > b.Score; });
    auto sorted = TakeTopActions(validActions, 4);
    if (sorted.empty()) return VMPrimitiveExitCode::GOTO_FALSE;
    auto selection = sorted[0];
    if (!selection.Entry->AutoFirst()) {
        // weighted random selection
        float totalScore = 0;
        for (auto& x : sorted) totalScore += x.Score;
        auto random = context->VM()->Context->NextRandom(10000);

        float randomTotal = 0;
        for (size_t i = 0; i < sorted.size(); i++) {
            randomTotal += (sorted[i].Score / totalScore) * 10000;
            if (random <= randomTotal) {
                selection = sorted[i];
                break;
            }
        }
    }

    std::vector<int16_t> pushArgs = { selection.Param0, 0, 0, 0 };
    auto qaction = selection.Callee->GetAction(selection.ID, context->Caller, context->VM()->Context, &pushArgs);
    if (qaction != nullptr) {
        qaction->Priority = (int16_t)VMQueuePriority::Autonomous;
        if (!selection.Name.empty()) qaction->Name = selection.Name; // the chosen action string (e.g. "Talk.../Gossip")
        context->Caller->Thread->EnqueueAction(qaction);
        context->StackObject = selection.Callee;
        if (context->VM()->OnAutonomyChoice) context->VM()->OnAutonomyChoice(context->Caller, sorted, selection); // sims diagnostic hook
        return VMPrimitiveExitCode::GOTO_TRUE;
    } else {
        return VMPrimitiveExitCode::GOTO_FALSE;
    }
}

VMPrimitiveExitCode VMFindBestAction::Execute(VMStackFrame* context, VMPrimitiveOperand* /*args*/) {
    if (context->VM()->TS1) return ExecuteTS1(context); // TS1(sims)
    auto ents = std::vector<VMEntity*>(context->VM()->Entities);
    auto processed = std::unordered_set<int16_t>();
    auto pos1 = context->Caller->Position();

    // C#: ((VMAvatar)context.Caller) — an invalid cast throws.
    auto callerAvatar = dynamic_cast<VMAvatar*>(context->Caller);

    VMEntity* bestActor = nullptr;
    VMPieMenuInteraction bestAction;
    bool hasBestAction = false; // C#: bestAction != null
    int32_t bestScore = 0;
    for (auto iobj : ents) {
        auto obj = iobj->MultitileGroup->GetInteractionGroupLeader(iobj);
        if (processed.count(obj->ObjectID)) continue;
        processed.insert(obj->ObjectID);

        auto pos2 = obj->Position();
        auto distance = (int16_t)std::floor(std::sqrt(std::pow(pos1.x - pos2.x, 2) + std::pow(pos1.y - pos2.y, 2)) / 16.0);

        auto pie = obj->GetPieMenu(context->VM(), context->Caller, true);
        int32_t bestMyScore = 0;
        const VMPieMenuInteraction* bestMyAction = nullptr;
        for (auto& item : pie) {
            //motive scores must be above their threshold.
            //pick maximum motive score as our base score
            int32_t baseScore = 0;
            for (size_t i = 0; i < item.Entry->MotiveEntries.size(); i++) {
                auto& motiveScore = item.Entry->MotiveEntries[i];
                if (motiveScore.EffectRangeMaximum == 0 && motiveScore.EffectRangeMinimum == 0) continue;
                //LINEAR INTERPOLATE MIN SCORE TO MAX, using motive data of caller
                //MAX is when motive is the lowest.
                //can be in reverse too!
                if (callerAvatar == nullptr) throw VMSimanticsException("Find Best Action: caller is not an avatar (invalid cast).", context);
                auto myMotive = std::max((int16_t)0, callerAvatar->GetMotiveData((VMMotive)i)); //below mid motives causes max atten right now.

                int32_t interpScore = (myMotive * motiveScore.EffectRangeMinimum + (100 - myMotive) * (motiveScore.EffectRangeMinimum + motiveScore.EffectRangeMaximum)) / 100;
                if (interpScore > baseScore) baseScore = interpScore;
            }
            if (item.Entry->AttenuationCode != 0 && item.Entry->AttenuationCode >= 5) throw VMSimanticsException("Find Best Action: attenuation code out of range.", context); // C# IndexOutOfRangeException
            float atten = (item.Entry->AttenuationCode == 0) ? item.Entry->AttenuationValue : TTAB::AttenuationValues[item.Entry->AttenuationCode];
            int32_t attenScore = (int32_t)std::max(0.0f, baseScore * (1.0f - (distance * atten)));
            if (attenScore != 0) attenScore += (int32_t)context->VM()->Context->NextRandom(31) - 15;
            // C# compares int with uint as long.
            if ((int64_t)attenScore > (int64_t)item.Entry->AutonomyThreshold && attenScore > bestMyScore) {
                bestMyScore = attenScore;
                bestMyAction = &item;
            }
        }

        if (bestMyAction != nullptr && bestMyScore > bestScore) {
            bestActor = obj;
            bestAction = *bestMyAction;
            hasBestAction = true;
            bestScore = bestMyScore;
        }
    }

    if (bestActor != nullptr && hasBestAction) {
        std::vector<int16_t> pushArgs = { bestAction.Param0, 0, 0, 0 };
        bestActor->PushUserInteraction(bestAction.ID, context->Caller, context->VM()->Context, &pushArgs);
        return VMPrimitiveExitCode::GOTO_TRUE;
    }

    return VMPrimitiveExitCode::GOTO_FALSE; //we couldn't find anything... because we didn't check! TODO!!
}

void VMFindBestActionOperand::Read(const uint8_t* bytes) {
    st::Reader io(bytes, 8, st::Endian::Little);
    (void)io;
}

void VMFindBestActionOperand::Write(uint8_t* /*bytes*/) { }

}}}
