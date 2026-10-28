/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
// sims app — TS1(sims): The Sims' cheat console (PC: Ctrl+Shift+C; here: hold L+R and press SELECT, or the
// "Cheat" button of the lot's Options panel). The box sits at the top of the top screen like the original's; the
// text comes from the console keyboard (libctru swkbd, cas/CASTextInput) or, on the host, from the script ("text").
//
// Syntax as in The Sims: cheats separated by ';', '!' repeats the previous cheat ("rosebud;!;!;!" = 4 x 1,000).
// Unknown text does nothing and prints nothing (the original's behaviour); a few cheats print a line.
// The cheat list and what each does in this port: docs/CHEATS.md.
#pragma once
#include <deque>
#include <string>
#include <vector>
#include "app/Canvas.h"
#include "app/Input.h"

namespace simapp {

class LotSession;
class LotScreen;
namespace front { class FrontEnd; }

class UICheatConsole {
public:
    // what the cheats act on (set by the App every frame; null when not in that state)
    LotSession* S = nullptr;
    LotScreen* L = nullptr;
    front::FrontEnd* FE = nullptr;
    std::string* Log = nullptr;

    bool Open = false;
    void Show();
    void Close();
    /// Runs the keyboard when due and answers A (type another cheat) / B (close). True = input consumed.
    bool Update(const AppInput& in);
    /// The cheat box at the top of the top screen (with the last lines entered and printed).
    void Draw(Canvas& top, double timeMs);

    /// Executes one line exactly as typed (';' chains, '!' repeats). Returns the number of cheats recognised.
    int Execute(const std::string& line);

    /// The last entered lines and the lines the cheats printed (newest last).
    std::deque<std::string> History;
    std::string LastCheat; // what '!' repeats

private:
    int _KeyboardIn = 0;   // frames until the keyboard opens (the box is drawn first)
    bool _MoneySound = false;
    bool ExecuteOne(const std::string& cheat, std::vector<std::string>& out);
    void Print(const std::string& s);
};

}
