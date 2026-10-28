/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
// sims — Create-A-Sim live preview (see CASSimPreview.h). TS1(sims).
#include "app/cas/CASSimPreview.h"
#include "app/cas/CASFamilyWriter.h"
#include "vitaboy/SimAvatar.h"
#include "vitaboy/Outfit.h"
#include "vitaboy/Animator.h"
#include "vitaboy/Skeleton.h"
#include "vitaboy/content/TS1AvatarProvider.h"
#include <algorithm>
#include <cmath>

namespace simapp { namespace cas {

using namespace FSO::Vitaboy;
using FSO::Content::TS1::TS1AvatarContent;

// Standing idles of the TS1 Sims (Animation.far): a breathing loop with the arms down, and the look-off /
// fidget variations that start and end in the same pose, so they chain without a pop.
static const char* ADULT_BASE = "a2o-idle-armsdown-breathe";
static const char* ADULT_VAR[] = { "a2o-idle-armsdown-lookoff", "a2o-idle-armsdown-fidget" };
static const char* CHILD_BASE = "c2o-idle-armsdown-breathe";
static const char* CHILD_VAR[] = { "c2o-idle-armsdown-lookoff", "c2o-idle-armsdown-fidget" };

CASSimPreview::CASSimPreview() : _Rng(12345) {}
CASSimPreview::~CASSimPreview() {}

void CASSimPreview::Set(const CASSimDesc& sim, bool* headOk, bool* bodyOk) {
    if (headOk) *headOk = true;
    if (bodyOk) *bodyOk = true;
    auto strings = sim.BodyStrings();
    std::string headKey = strings[2];
    std::string bodyKey = strings[1];
    for (int i = 17; i <= 22; i++) bodyKey += "|" + strings[(size_t)i];
    std::string skelName = sim.Age == CASAge::Child ? "child.skel" : "adult.skel";
    auto content = TS1AvatarContent::Get();
    if (!content->EnsureInit()) { _Avatar.reset(); return; }

    bool rebuilt = false;
    if (!_Avatar || skelName != _SkelName) {
        auto skel = content->BCF.GetSkeleton(skelName);
        if (!skel) { _Avatar.reset(); return; }
        _Avatar.reset(); // outfits are referenced by the avatar: drop it first
        _Avatar.reset(new SimAvatar(skel));
        _SkelName = skelName;
        _Child = sim.Age == CASAge::Child;
        _HeadKey.clear(); _BodyKey.clear();
        rebuilt = true;
        _Anim = nullptr;
    }
    if (headKey != _HeadKey) {
        std::unique_ptr<Outfit> o(new Outfit());
        o->ReadHead(headKey);
        _Avatar->Head = o.get();
        _Head = std::move(o);
        _HeadKey = headKey;
        rebuilt = true;
    }
    if (bodyKey != _BodyKey) {
        std::string hands[6];
        for (int i = 0; i < 6; i++) hands[i] = strings[(size_t)(17 + i)];
        std::unique_ptr<Outfit> o(new Outfit());
        o->Read(strings[1], hands);
        _Avatar->Body = o.get();
        _Avatar->Handgroup = o.get();
        _Body = std::move(o);
        _BodyKey = bodyKey;
        rebuilt = true;
    }
    // which parts bound? (a body binding is anything that is not the head; heads bind to HEAD)
    bool haveHead = false, haveBody = false;
    for (auto& b : _Avatar->Bindings) {
        if (!b || !b->Mesh) continue;
        std::string n = b->Mesh->SkinName;
        for (auto& ch : n) ch = (char)tolower((unsigned char)ch);
        if (n.find("head") != std::string::npos) haveHead = true; else if (n.find("hand") == std::string::npos) haveBody = true;
    }
    if (headOk) *headOk = haveHead;
    if (bodyOk) *bodyOk = haveBody;
    _Key = headKey + "#" + bodyKey + "#" + skelName;
    if (rebuilt) {
        if (!_Anim) ResetIdle();
        Measure();
        Pose();
    }
}

void CASSimPreview::ResetIdle() {
    _Loops = 0;
    PickIdle(true);
}

void CASSimPreview::PickIdle(bool base) {
    auto content = TS1AvatarContent::Get();
    const char* name = _Child ? CHILD_BASE : ADULT_BASE;
    if (!base) {
        int n = (int)(sizeof(ADULT_VAR) / sizeof(ADULT_VAR[0]));
        name = (_Child ? CHILD_VAR : ADULT_VAR)[_Rng() % (unsigned)n];
    }
    Animation* a = content->BCF.GetAnimation(std::string(name) + ".anim");
    if (!a) a = content->BCF.GetAnimation(_Child ? "c2o-standing-loop.anim" : "a2o-standing-loop.anim");
    _Anim = a;
    _AnimMs = 0;
}

void CASSimPreview::Pose() {
    if (!_Avatar) return;
    _Avatar->ResetPose();
    if (_Anim && _Anim->NumFrames() > 0) {
        float fps = _Anim->Duration > 0 ? (float)_Anim->NumFrames() * 1000.0f / _Anim->Duration : 30.0f;
        float f = (float)(_AnimMs * fps / 1000.0);
        int fi = (int)f;
        if (fi >= _Anim->NumFrames()) fi = _Anim->NumFrames() - 1;
        Animator::RenderFrame(_Avatar.get(), _Anim, fi, f - (float)(int)f, 1.0f);
    }
    _Avatar->ReloadSkeleton();
}

void CASSimPreview::Update(double ms) {
    if (!_Avatar) return;
    if (!_Anim) PickIdle(true);
    _AnimMs += ms;
    if (_Anim && _Anim->Duration > 0 && _AnimMs >= _Anim->Duration) {
        _Loops++;
        // breathe 2-4 times, then a look-off/fidget, then back to breathing
        bool wasBase = _Anim->Name.find("breathe") != std::string::npos;
        if (wasBase && _Loops >= 2 + (int)(_Rng() % 3)) { _Loops = 0; PickIdle(false); }
        else if (!wasBase) PickIdle(true);
        else _AnimMs = std::fmod(_AnimMs, (double)_Anim->Duration);
    }
    Pose();
}

void CASSimPreview::Measure() {
    if (!_Avatar || !_Avatar->Skeleton) return;
    _Avatar->ResetPose();
    _Avatar->ReloadSkeleton();
    float top = 0;
    for (auto& b : _Avatar->Skeleton->Bones) top = std::max(top, b->AbsolutePosition.Y);
    auto head = _Avatar->Skeleton->GetBone("HEAD");
    if (head) _HeadY = head->AbsolutePosition.Y;
    // the HEAD bone sits at the base of the skull; the crown is about 0.6 units higher for adults
    _HeadSize = _Child ? 0.75f : 0.95f;
    _Height = std::max(top, _HeadY + _HeadSize);
    if (_Height <= 0.5f) _Height = 5.4f;
}

// ---- tilt: the vitaboy camera is the game's 30-degree iso view; CAS looks at the Sim nearly level, so the whole
// skeleton is pitched towards the camera about the horizontal axis perpendicular to the view (around the feet).
static Quaternion QMul(const Quaternion& a, const Quaternion& b) {
    return Quaternion(a.W * b.X + a.X * b.W + a.Y * b.Z - a.Z * b.Y,
                      a.W * b.Y - a.X * b.Z + a.Y * b.W + a.Z * b.X,
                      a.W * b.Z + a.X * b.Y - a.Y * b.X + a.Z * b.W,
                      a.W * b.W - a.X * b.X - a.Y * b.Y - a.Z * b.Z);
}
static Quaternion QAxis(const Vector3& axis, float angle) {
    float s = std::sin(angle / 2);
    return Quaternion(axis.X * s, axis.Y * s, axis.Z * s, std::cos(angle / 2));
}
static float MDiff(const Matrix& a, const Matrix& b) {
    const float* x = &a.M11; const float* y = &b.M11;
    float d = 0;
    for (int i = 0; i < 16; i++) d += std::fabs(x[i] - y[i]);
    return d;
}

bool CASSimPreview::ApplyTilt(float tilt, float direction) {
    if (!_Avatar || !_Avatar->Skeleton || !_Avatar->Skeleton->RootBone || tilt == 0) return false;
    Bone* root = _Avatar->Skeleton->RootBone;
    _SavedRot = root->Rotation; _SavedTrans = root->Translation;
    // world horizontal axis perpendicular to the TopLeft camera's view direction, into avatar-local space
    Matrix toWorld = Matrix::CreateRotationY((float)(M_PI - direction));
    Vector3 w(0.70710678f * _TiltSign, 0, -0.70710678f * _TiltSign);
    Vector3 axis(w.X * toWorld.M11 + w.Y * toWorld.M12 + w.Z * toWorld.M13,
                 w.X * toWorld.M21 + w.Y * toWorld.M22 + w.Z * toWorld.M23,
                 w.X * toWorld.M31 + w.Y * toWorld.M32 + w.Z * toWorld.M33); // * transpose = inverse rotation
    Quaternion qt = QAxis(axis, tilt);
    Matrix tiltM = Matrix::CreateFromQuaternion(qt);
    _TiltM = tiltM;
    Matrix target = Matrix::CreateFromQuaternion(root->Rotation) * tiltM;
    Quaternion a = QMul(root->Rotation, qt), b = QMul(qt, root->Rotation);
    root->Rotation = MDiff(Matrix::CreateFromQuaternion(a), target) < MDiff(Matrix::CreateFromQuaternion(b), target) ? a : b;
    root->Translation = Transform(root->Translation, tiltM);
    _Avatar->ReloadSkeleton();
    return true;
}

void CASSimPreview::RestoreTilt() {
    if (!_Avatar || !_Avatar->Skeleton || !_Avatar->Skeleton->RootBone) return;
    _Avatar->Skeleton->RootBone->Rotation = _SavedRot;
    _Avatar->Skeleton->RootBone->Translation = _SavedTrans;
    _Avatar->ReloadSkeleton();
}

AvatarDrawResult CASSimPreview::DrawAnchored(Canvas& c, const Rect& clip, const st::Vector3& local, float sx, float sy,
                                             float pxPerUnit, float direction, bool bilinear) {
    AvatarDrawResult r;
    if (!_Avatar || c.Width <= 0) return r;
    if (_Z.size() < (size_t)c.Width * c.Height) _Z.assign((size_t)c.Width * c.Height, 1.0f);
    Rect cl(std::max(0, clip.X), std::max(0, clip.Y), 0, 0);
    cl.W = std::min(c.Width, clip.X + clip.W) - cl.X;
    cl.H = std::min(c.Height, clip.Y + clip.H) - cl.Y;
    if (cl.W <= 0 || cl.H <= 0) return r;
    for (int y = cl.Y; y < cl.Y + cl.H; y++) std::fill(&_Z[(size_t)y * c.Width + cl.X], &_Z[(size_t)y * c.Width + cl.X + cl.W], 1.0f);

    WorldCamera cam;
    cam.Zoom = WorldZoom::Near;
    // Near zoom: one world unit along a horizontal axis is 128/(3*sqrt2) px at PreciseZoom 1
    cam.PreciseZoom = pxPerUnit / (128.0f / (3.0f * 1.41421356f));
    Vector3 pos(10.5f, 10.5f, 0.0f);
    cam.CenterTile = Vector2(pos.X, pos.Y);
    cam.ViewDimensions = Vector2((float)c.Width, (float)c.Height);
    Matrix world = Matrix::CreateRotationY((float)(M_PI - direction)) * Matrix::CreateTranslation(WorldCamera::GetWorldFromTile(pos));
    if (_TiltSign == 0) {
        // which way is "towards the camera"? the tilted head must rise on screen
        auto head = _Avatar->Skeleton ? _Avatar->Skeleton->GetBone("HEAD") : nullptr;
        _TiltSign = 1;
        if (head) {
            float y0 = cam.ProjectWorld(Transform(head->AbsolutePosition, world)).Y;
            ApplyTilt(0.4f, direction);
            float y1 = cam.ProjectWorld(Transform(head->AbsolutePosition, world)).Y;
            RestoreTilt();
            if (y1 > y0) _TiltSign = -1;
        }
    }
    bool tilted = ApplyTilt(_TiltAngle, direction);
    // where does the (tilted) anchor land without offset? (same world transform as AvatarComponent.Draw)
    Vector3 anchor = tilted ? Transform(local, _TiltM) : local;
    Vector3 sp = cam.ProjectWorld(Transform(anchor, world));
    cam.PixelOffset = Vector2(sx - sp.X, sy - sp.Y);

    Framebuffer fb(c.Width, c.Height, (uint8_t*)c.Px.data());
    ZBuffer zb(c.Width, c.Height, _Z.data());
    AvatarDrawOptions o;
    o.Bilinear = bilinear;
    o.Clip = st::Rectangle(cl.X, cl.Y, cl.W, cl.H);
    r = DrawAvatar(fb, zb, cam, *_Avatar, pos, direction, o);
    if (tilted) RestoreTilt();
    return r;
}

// The renderer's convention: RadianDirection 0 faces away from the TopLeft camera; the CAS "front" is PI/4 turned
// so the Sim looks straight at the viewer.
static const float FRONT = (float)(M_PI * 0.75);

AvatarDrawResult CASSimPreview::Draw(Canvas& c, const Rect& clip, float footX, float footY, float heightPx, float direction, bool bilinear) {
    // vertical world units appear cos(30deg) shorter in the 30-degree camera
    float pxPerUnit = heightPx / (_Height * std::cos(0.5236f - _TiltAngle));
    return DrawAnchored(c, clip, st::Vector3(0, 0, 0), footX, footY, pxPerUnit, FRONT + direction, bilinear);
}

AvatarDrawResult CASSimPreview::DrawHead(Canvas& c, const Rect& clip, float cx, float cy, float headPx, float direction, bool bilinear) {
    float pxPerUnit = headPx / (_HeadSize * 1.25f);
    // HEAD bone is at the neck; the face centre is ~0.45 of the head size above it
    return DrawAnchored(c, clip, st::Vector3(0, _HeadY + _HeadSize * 0.30f, 0), cx, cy, pxPerUnit, FRONT + direction, bilinear);
}

void CASSimPreview::RenderPortraits(CASPortraits& out) {
    if (!_Avatar) return;
    // neutral pose
    auto savedAnim = _Anim; double savedMs = _AnimMs;
    _Anim = nullptr;
    _Avatar->ResetPose();
    _Avatar->ReloadSkeleton();

    float savedTilt = _TiltAngle;
    _TiltAngle = 0.52f; // 30 degrees: a level, frontal portrait like the shipped ones
    auto fill = [](Canvas& cv, int w, int h, uint32_t col) { cv.Resize(w, h); cv.Clear(col); };
    // faces: three 35x41 views (turned right, front, turned left) on dark blue (0,0,80)
    fill(out.Faces, 105, 41, RGBA(0, 0, 80));
    const float views[3] = { -0.55f, 0.0f, 0.55f };
    for (int i = 0; i < 3; i++) DrawHead(out.Faces, Rect(i * 35, 0, 35, 41), i * 35 + 17.5f, 22.0f, 38.0f, views[i], true);
    // relationship images: the face on the five relationship colours (enemy .. best friend)
    fill(out.RelImages, 200, 40, RGBA(0, 0, 0));
    const uint32_t relCol[5] = { RGBA(208, 0, 0), RGBA(136, 44, 40), RGBA(64, 92, 88), RGBA(32, 144, 72), RGBA(0, 200, 56) };
    for (int i = 0; i < 5; i++) {
        out.RelImages.FillRect(i * 40, 0, 40, 40, relCol[i]);
        DrawHead(out.RelImages, Rect(i * 40, 0, 40, 40), i * 40 + 20.0f, 22.0f, 40.0f, 0.0f, true);
    }
    fill(out.Thumbnail, 25, 25, RGBA(0, 0, 248));
    DrawHead(out.Thumbnail, Rect(0, 0, 25, 25), 12.5f, 13.5f, 24.0f, 0.0f, true);
    fill(out.SpeechMed, 16, 16, RGBA(248, 252, 248));
    DrawHead(out.SpeechMed, Rect(0, 0, 16, 16), 8.0f, 8.5f, 16.0f, -0.4f, true);
    fill(out.SpeechLarge, 32, 32, RGBA(248, 252, 248));
    DrawHead(out.SpeechLarge, Rect(0, 0, 32, 32), 16.0f, 17.0f, 32.0f, -0.4f, true);

    _TiltAngle = savedTilt;
    _Anim = savedAnim; _AnimMs = savedMs;
    Pose();
}

}}
