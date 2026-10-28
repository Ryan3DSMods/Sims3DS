/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
// Translated from FreeSims sims.files/HIT/Hot.cs
#include "files/HIT/Hot.h"
#include "files/HIT/HITText.h"

namespace FSO { namespace Files { namespace HIT {

Hot::Hot(const std::vector<uint8_t>& FileData)
{
    bool EventState = false, OptionsState = false, TrackDataState = false;

    for (auto& raw : Text::ReadLines(FileData))
    {
        std::string CurrentLine = Text::Trim(raw);

        if (CurrentLine == "[EventMappingEquate]") EventState = true;
        else if (CurrentLine == "[Options]") OptionsState = true;
        else if (CurrentLine == "[TrackData]") TrackDataState = true;

        if (!Text::Contains(CurrentLine, "["))
        {
            if (!Text::Contains(CurrentLine, "]"))
            {
                // (sic) C#: if (!CurrentLine.Contains("")) is never true, so this constructor reads nothing.
                if (false)
                {
                    (void)EventState; (void)OptionsState; (void)TrackDataState;
                }
            }
        }
    }
}

void Hot::Load(const std::vector<uint8_t>& FileData)
{
    HotReadMode ActiveState = HotReadMode::None;

    for (auto& raw : Text::ReadLines(FileData))
    {
        std::string CurrentLine = Text::Trim(raw);

        if (CurrentLine == "[EventMappingEquate]") ActiveState = HotReadMode::EventMappingEquate;
        else if (CurrentLine == "[Options]") ActiveState = HotReadMode::Options;
        else if (CurrentLine == "[TrackData]") ActiveState = HotReadMode::TrackData;
        else if (CurrentLine == "[EventMapping]")
            //equivalent to .evt file
            //(name)=(eventMappingEquate as event type),(trackid),0,0,0,0
            ActiveState = HotReadMode::EventMapping;
        else if (CurrentLine == "[Track]")
            //equivalent to a lot of track files
            //(trackid)=0,(subroutine),(volume),(arguments),(duckingPriority),(controlGroup),(soundPressureLevel),@(hitlistID),(patchID)
            //patch id is usually 0, in favor of single item hitlists
            ActiveState = HotReadMode::Track;
        else if (CurrentLine == "[Patch]")
            //(patchid)=(name),(filenameInQuotes),(looped),(piano),0,0,0
            ActiveState = HotReadMode::Patch;
        else if (CurrentLine == "[GlobalHitList]")
            //(hitlistid)=(hitlistString)
            //note: many hitlists contain just one patch
            ActiveState = HotReadMode::GlobalHitList;

        if (Text::Contains(CurrentLine, "[") || Text::Contains(CurrentLine, "]") || CurrentLine == "") continue;

        auto Params = Text::Split(CurrentLine, '=');
        if (Params.size() < 2) continue; // C#: IndexOutOfRange
        // TS1(sims): a malformed line is skipped instead of aborting the whole bank (the C# threw out of Load).
        try
        {
            switch (ActiveState)
            {
            //EventMappingEquate fields look like: kSndobPlay=1
            case HotReadMode::EventMappingEquate:
            {
                FSO::Files::HIT::EventMappingEquate EMappingEquate;
                EMappingEquate.Label = Params[0];
                EMappingEquate.Value = Text::ToInt32(Params[1]);
                EventMappingEquate_[EMappingEquate.Label] = EMappingEquate.Value;
                m_EventMappingEquations.push_back(EMappingEquate); // TS1(sims): the C# never filled the list
                break;
            }
            //Options fields look like: Version=1
            case HotReadMode::Options:
                if (Params[0] == "Version") m_Version = Text::ToInt32(Params[1]);
                else if (Params[0] == "LoadPriority") m_LoadPriority = Text::ToInt32(Params[1]);
                break;
            //TrackData fields look like: 0xb0f4=0x10
            case HotReadMode::TrackData:
                TrackData.emplace(Text::ToUInt32Hex(Params[0]), Text::ToUInt32Hex(Params[1])); // C# Dictionary.Add
                break;
            case HotReadMode::EventMapping:
            {
                auto commaSplit = Text::Split(Params[1], ',');
                EVTEntry e;
                e.Name = Text::ToLower(Params[0]);
                e.EventType = (uint32_t)ParseEME(commaSplit[0]);
                e.TrackID = (commaSplit.size() > 1) ? (uint32_t)ParseEME(commaSplit[1]) : 0;
                // TS1(sims): keep the extra columns (kSequenceTrackHitList carries a hitlist in the 4th)
                if (commaSplit.size() > 2) e.Unknown = (uint32_t)ParseEME(commaSplit[2]);
                if (commaSplit.size() > 3) e.Unknown2 = (uint32_t)ParseEME(commaSplit[3]);
                if (commaSplit.size() > 4) e.Unknown3 = (uint32_t)ParseEME(commaSplit[4]);
                Events[e.Name] = e;
                break;
            }
            case HotReadMode::Track:
            {
                auto tid = Text::ToUInt32(Params[0]);
                auto tcSplit = Text::Split(Params[1], ',');
                if (tcSplit.size() < 9) break;

                FSO::Files::HIT::Track trk;
                trk.SubroutineID = 0;//(uint)HSMConst(tcSplit[1]),
                trk.TrackName = tcSplit[1]; // TS1(sims): kept for logs
                trk.TrackID = tid;          // TS1(sims)
                trk.Volume = (uint32_t)ParseEME(tcSplit[2]);
                trk.ArgType = (HITArgs)ParseEME(tcSplit[3]);
                trk.DuckingPriority = (HITDuckingPriorities)ParseEME(tcSplit[4]);
                trk.ControlGroup = (HITControlGroups)ParseEME(tcSplit[5]);
                trk.HitlistID = (uint32_t)HSMConst(tcSplit[7].size() > 0 ? tcSplit[7].substr(1) : tcSplit[7]); //cut out @
                // TS1(sims): the patch column is a number ("1229") or an HSM label ("@PchCounterPut");
                // the C# ParseEME returned 0 for the label form.
                trk.SoundID = Text::StartsWith(tcSplit[8], "@") ? (uint32_t)HSMConst(tcSplit[8].substr(1)) : (uint32_t)ParseEME(tcSplit[8]);

                if (trk.HitlistID != 0 && TrackData.count(trk.HitlistID)) trk.SubroutineID = TrackData[trk.HitlistID];
                // TS1(sims): the C# also did `if (SoundID != 0 && TrackData.ContainsKey(SoundID)) SubroutineID =
                // TrackData[SoundID]` (TSO). In TS1 banks SoundID is a patch id and the TrackData keys are label ids
                // from the same number range, so that override jumped into unrelated code; it is not applied.

                Tracks[tid] = trk;
                break;
            }
            case HotReadMode::Patch:
            {
                auto pid = Text::ToUInt32(Params[0]);
                Patches[pid] = FSO::Files::HIT::Patch(Params[1]);
                break;
            }
            case HotReadMode::GlobalHitList:
            {
                auto hid = Text::ToUInt32(Params[0]);
                try
                {
                    auto hitlist = Hitlist::HitlistFromString(Params[1]);
                    Hitlists[hid] = hitlist;
                }
                catch (std::exception&)
                {
                    /*
                     * todo: saxophone seems to reference an hsm.
                     * 20016=sulsaxj_way_aa
                     * 20017=sulsaxk_way_solo
                     * 20018=sulsaxl_way_fin
                     * these labels are in the hsm but they have a different case...
                     */
                }
                break;
            }
            default:
                break;
            }
        }
        catch (std::exception&)
        {
        }
    }
}

int32_t Hot::HSMConst(const std::string& input)
{
    if (AsmNames == nullptr) return 0;
    // TS1(sims): HSM names are stored lower case (HSM.cs ToLowerInvariant) but the HOT spells them in mixed
    // case ("@tkd_JugglerTada"), so the C# lookup only matched all-lowercase labels.
    auto it = AsmNames->Constants.find(Text::ToLower(input));
    return (it != AsmNames->Constants.end()) ? it->second : 0;
}

int32_t Hot::ParseEME(const std::string& eme)
{
    int32_t result = 0;
    if (Text::TryParseInt(eme, result)) return result;
    auto it = EventMappingEquate_.find(Text::Trim(eme));
    return (it != EventMappingEquate_.end()) ? it->second : 0;
}

Hot::Hot(const std::string& Filepath, std::shared_ptr<HSM> myAsm)
{
    AsmNames = myAsm;
    Load(Text::ReadAllBytes(Filepath));
}

}}} // namespace FSO::Files::HIT
