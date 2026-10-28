/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
// FSO.Vitaboy.BCF (sims.common/rendering/vitaboy/BCF.cs)
#include "vitaboy/BCF.h"
#include "vitaboy/BCFReadProxy.h"

namespace FSO { namespace Vitaboy {

BCF::BCF(const std::vector<uint8_t>& data, bool cmx) {
    std::unique_ptr<IOProxy> proxy;
    if (cmx) proxy.reset(new CMXReadProxy(data, true));
    else proxy.reset(new BinaryReadProxy(data, st::Endian::Little));
    auto& io = *proxy;

    auto skelCount = io.ReadInt32();
    for (int32_t i = 0; i < skelCount; i++) {
        std::unique_ptr<Skeleton> s(new Skeleton());
        s->Read(io, true);
        Skeletons.push_back(std::move(s));
    }
    auto aprCount = io.ReadInt32();
    for (int32_t i = 0; i < aprCount; i++) {
        std::unique_ptr<Appearance> a(new Appearance());
        a->ReadBCF(io);
        Appearances.push_back(std::move(a));
    }
    // some hand-made text CMX files end after the suit list
    if (!io.HasMore()) return;
    auto animCount = io.ReadInt32();
    for (int32_t i = 0; i < animCount; i++) {
        std::unique_ptr<Animation> a(new Animation());
        a->Read(io, true);
        Animations.push_back(std::move(a));
    }
}

}}
