/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
// sims audio — host WAV capture backend.
#include "audio/AudioBackend.h"

namespace simaudio {

bool WavCaptureBackend::Start(AudioMixer& mixer) {
    _Mixer = &mixer;
    return _W.Begin(_Path, 2, mixer.OutputRate());
}

void WavCaptureBackend::Stop() {
    _W.End();
    _Mixer = nullptr;
}

void WavCaptureBackend::Pump(double dtMs) {
    if (_Mixer == nullptr) return;
    _Frac += dtMs * _Mixer->OutputRate() / 1000.0;
    int frames = (int)_Frac;
    _Frac -= frames;
    while (frames > 0) {
        int n = frames > 1024 ? 1024 : frames;
        _Buf.resize((size_t)n * 2);
        _Mixer->Mix(_Buf.data(), n);
        if (_Mixer->LastPeak() > _Peak) _Peak = _Mixer->LastPeak();
        _W.Append(_Buf.data(), (size_t)n);
        frames -= n;
    }
}

double WavCaptureBackend::SecondsWritten() const {
    return _Mixer ? (double)_W.FramesWritten() / _Mixer->OutputRate() : 0.0;
}

}
