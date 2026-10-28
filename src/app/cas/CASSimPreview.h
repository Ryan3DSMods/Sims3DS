/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
// sims — the live 3D Sim of Create-A-Sim: a Vitaboy SimAvatar dressed from the CAS choices, playing the
// standing idles, drawn with the ported software rasterizer (vitaboy/AvatarRenderer) into an app Canvas.
// Also renders the portraits a character file stores (faces, relationship images, thumbnail, speech icons).
//
// FreeSims: PersonSelectionEdit used a UISim (SimBox.AutoRotate = true; Avatar.Head/Body/Handgroup = outfit;
// Avatar.Appearance = skin tone). Here the outfits are TS1 body strings (Outfit::Read / ReadHead), as VMAvatar
// builds them for a Sim on a lot.
#pragma once
#include <cstdint>
#include <memory>
#include <random>
#include <string>
#include <vector>
#include "app/Canvas.h"
#include "app/cas/CASSimDesc.h"
#include "vitaboy/AvatarRenderer.h"

namespace FSO { namespace Vitaboy { class SimAvatar; class Outfit; class Animation; class Skeleton; } }

namespace simapp { namespace cas {

struct CASPortraits;

class CASSimPreview {
public:
    CASSimPreview();
    ~CASSimPreview();

    /// Dresses the avatar (rebuilds only what changed). headOk/bodyOk report meshes that failed to bind.
    void Set(const CASSimDesc& sim, bool* headOk = nullptr, bool* bodyOk = nullptr);
    bool Valid() const { return _Avatar != nullptr; }
    const std::string& Key() const { return _Key; }

    /// Advances the idle animation (ms of real time).
    void Update(double ms);
    /// Restarts the idles from the neutral breathing loop (e.g. when the Sim is changed).
    void ResetIdle();

    /// Draws the Sim standing with its feet at (footX, footY) of the canvas, `heightPx` tall (approx.),
    /// turned by `direction` radians (0 = facing the viewer), clipped to `clip`. Depth is local to the call.
    FSO::Vitaboy::AvatarDrawResult Draw(Canvas& c, const Rect& clip, float footX, float footY, float heightPx,
                                        float direction, bool bilinear = true);
    /// Draws the head only region: the HEAD bone at (cx, cy), `pxPerUnit` pixels per world unit.
    FSO::Vitaboy::AvatarDrawResult DrawHead(Canvas& c, const Rect& clip, float cx, float cy, float headPx,
                                            float direction, bool bilinear = true);

    /// Renders the five portrait images of a character file, in the neutral standing pose.
    void RenderPortraits(CASPortraits& out);

    /// Pitch towards the camera (radians, 0 = the game's 30-degree view; 0.52 = level). Default 0.3.
    void SetTilt(float radians) { _TiltAngle = radians; }

    /// Standing height (feet to top of head) in world units, measured from the mesh.
    float StandingHeight() const { return _Height; }

private:
    std::string _Key, _HeadKey, _BodyKey, _SkelName;
    std::unique_ptr<FSO::Vitaboy::SimAvatar> _Avatar;
    std::unique_ptr<FSO::Vitaboy::Outfit> _Head, _Body;
    std::vector<float> _Z;
    float _Height = 5.4f, _HeadY = 4.9f, _HeadSize = 1.2f;
    bool _Child = false;
    float _TiltAngle = 0.3f;
    int _TiltSign = 0;
    FSO::Vitaboy::Quaternion _SavedRot;
    st::Vector3 _SavedTrans;
    FSO::Vitaboy::Matrix _TiltM;
    bool ApplyTilt(float tilt, float direction);
    void RestoreTilt();

    // idles
    FSO::Vitaboy::Animation* _Anim = nullptr;
    double _AnimMs = 0;
    int _Loops = 0;
    std::mt19937 _Rng;
    void PickIdle(bool base);
    void Pose();
    void Measure();
    FSO::Vitaboy::AvatarDrawResult DrawAnchored(Canvas& c, const Rect& clip, const st::Vector3& localAnchor, float sx, float sy,
                                                float pxPerUnit, float direction, bool bilinear);
};

}}
