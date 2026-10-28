/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
// FSO.Vitaboy.SimAvatar (SimsVille/Vitaboy/SimAvatar.cs) + AdultVitaboyModel.cs
#include "vitaboy/SimAvatar.h"
#include "vitaboy/Appearance.h"
#include "vitaboy/content/TS1AvatarProvider.h"

namespace FSO { namespace Vitaboy {

using FSO::Content::TS1::TS1AvatarContent;
using FSO::Common::Content::ContentID;

/// Content.AvatarAppearances.Get(ContentID): TS1 ids carry a file name ("b001mafit_01.apr").
static FSO::Vitaboy::Appearance* GetAppearanceByID(const ContentID& id) {
    if (!id.HasFileName) return nullptr; // STUB(3ds): TSO type/file id appearances
    auto content = TS1AvatarContent::Get();
    if (!content->EnsureInit()) return nullptr;
    return content->BCF.GetAppearance(id.FileName);
}

SimAvatar::SimAvatar(FSO::Vitaboy::Skeleton* skel) : Avatar(skel) {
}

SimAvatar::SimAvatar(const SimAvatar& old) : Avatar(old.BaseSkeleton) {
    Handgroup.SetSilent(old.Handgroup.Value());
    Body.SetSilent(old.Body.Value());
    Head.SetSilent(old.Head.Value());
    Appearance.SetSilent(old.Appearance.Value());

    ReloadSkeleton();
    ReloadHead();
    ReloadBody();
    ReloadHandgroup();
}

void SimAvatar::StripAllButHead() {
    if (Handgroup.Value() != nullptr) {
        RemoveAppearance(m_LeftHandInstance, true);
        RemoveAppearance(m_RightHandInstance, true);
    }
    RemoveAppearance(m_BodyInstance, true);
}

void SimAvatar::ReloadHandgroup() {
    if (m_LeftHandInstance != nullptr)
        RemoveAppearance(m_LeftHandInstance, true);
    if (m_RightHandInstance != nullptr)
        RemoveAppearance(m_RightHandInstance, true);
    m_LeftHandInstance = nullptr;
    m_RightHandInstance = nullptr;

    Outfit* handgroup = Handgroup.Value();
    if (handgroup != nullptr) {
        // TS1: the outfit carries a literal handgroup built from the body strings (FreeSO).
        // STUB(3ds): otherwise Content.AvatarHandgroups.Get(GetHandgroup()) - TSO .hag files.
        FSO::Vitaboy::HandGroup* Handgroup = handgroup->LiteralHandgroup.get();
        if (Handgroup == nullptr) return;

        ContentID LeftID, RightID;
        FSO::Vitaboy::HandSet* HSet = nullptr;
        std::string LeftTex, RightTex;

        switch (Appearance.Value()) {
            case AppearanceType::Light: HSet = &Handgroup->LightSkin; break;
            case AppearanceType::Medium: HSet = &Handgroup->MediumSkin; break;
            case AppearanceType::Dark: HSet = &Handgroup->DarkSkin; break;
        }
        // TS1 body strings only fill one hand set (the texture names already encode the skin tone).
        if (handgroup->LiteralHandgroup != nullptr) HSet = &Handgroup->LightSkin;

        switch (LeftHandGesture.Value()) {
            case SimHandGesture::Idle:
                LeftID = HSet->LeftHand.Idle.ID();
                LeftTex = HSet->LeftHand.Idle.TexName;
                break;
            case SimHandGesture::Fist:
                LeftID = HSet->LeftHand.Fist.ID();
                LeftTex = HSet->LeftHand.Idle.TexName;
                break;
            case SimHandGesture::Pointing:
                LeftID = HSet->LeftHand.Pointing.ID();
                LeftTex = HSet->LeftHand.Idle.TexName;
                break;
            default: break; // None: no hand
        }

        switch (RightHandGesture.Value()) {
            case SimHandGesture::Idle:
                RightID = HSet->RightHand.Idle.ID();
                RightTex = HSet->RightHand.Idle.TexName;
                break;
            case SimHandGesture::Fist:
                RightID = HSet->RightHand.Fist.ID();
                RightTex = HSet->RightHand.Idle.TexName;
                break;
            case SimHandGesture::Pointing:
                RightID = HSet->RightHand.Pointing.ID();
                RightTex = HSet->RightHand.Idle.TexName;
                break;
            default: break;
        }

        auto LeftApr = GetAppearanceByID(LeftID);
        auto RightApr = GetAppearanceByID(RightID);

        if (LeftApr != nullptr)
            m_LeftHandInstance = AddAppearance(LeftApr, LeftTex);
        if (RightApr != nullptr)
            m_RightHandInstance = AddAppearance(RightApr, RightTex);
    }
}

void SimAvatar::ReloadHead() {
    if (m_HeadInstance != nullptr) {
        RemoveAppearance(m_HeadInstance, true);
        m_HeadInstance = nullptr;
    }
    Outfit* head = Head.Value();
    if (head != nullptr) {
        auto AppearanceID = head->GetAppearance(Appearance.Value());
        auto apr = GetAppearanceByID(AppearanceID);
        if (apr != nullptr) {
            m_HeadInstance = AddAppearance(apr, head->TS1TextureID);
        }
    }
}

void SimAvatar::ReloadBody() {
    if (m_BodyInstance != nullptr) {
        RemoveAppearance(m_BodyInstance, true);
        m_BodyInstance = nullptr;
    }
    Outfit* body = Body.Value();
    if (body != nullptr) {
        auto AppearanceID = body->GetAppearance(Appearance.Value());
        auto apr = GetAppearanceByID(AppearanceID);
        if (apr != nullptr) {
            m_BodyInstance = AddAppearance(apr, body->TS1TextureID);
        }
    }
}

AdultVitaboyModel::AdultVitaboyModel()
    : SimAvatar(TS1AvatarContent::Get()->EnsureInit() ? TS1AvatarContent::Get()->BCF.GetSkeleton("adult.skel") : nullptr) {
}

}}
