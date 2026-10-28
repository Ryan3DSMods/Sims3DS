/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
// sims — loading progress / step log hooks shared by the content loaders and the app.
//
// Loaders call Step() when a named loading step starts and Progress() inside long steps; the app installs the sinks:
// the 3DS writes a timestamped line with free memory to log.txt per step (when sdmc:/3ds/sims/debug exists) and
// redraws the loading screen with the progress, so a slow SD card never looks like a hang.
#pragma once
#include <cstddef>
#include <functional>
#include <string>

namespace FSO { namespace Files {

struct LoadProgress {
    /// A loading step starts ("objects: index", "lot 5: objects"...).
    static void Step(const std::string& what) { Current = what; if (StepSink) StepSink(what); }
    /// The step running now (for nested steps that hand back, e.g. the avatar index inside a lot load).
    static std::string Current;
    /// Progress inside the current step (total 0 = unknown). Cheap to call often: the sink throttles redraws.
    static void Progress(const std::string& what, size_t done, size_t total) { if (ProgressSink) ProgressSink(what, done, total); }

    static std::function<void(const std::string&)> StepSink;
    static std::function<void(const std::string&, size_t, size_t)> ProgressSink;
};

}} // namespace FSO::Files
