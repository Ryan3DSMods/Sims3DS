/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
// simrun — boots the translated FreeSims engine on the original game data.
//   simrun <gameDataDir> <cacheDir> <House.iff> [ticks]
// gameDataDir must contain GameData/Objects/Objects.far and GameData/Global/Global.far.
#include "content/ContentManager.h"
#include "simantics/VM.h"
#include "simantics/VMContext.h"
#include "simantics/utils/VMWorldActivator.h"
#include "simantics/entities/VMEntity.h"
#include "simantics/entities/VMAvatar.h"
#include "simantics/engine/VMThread.h"
#include "files/formats/iff/IffFile.h"
#include <cstdio>
#include <cstdlib>
#include <map>
using namespace FSO::SimAntics;

int main(int argc, char** argv) {
    if (argc < 4) { printf("usage: simrun <gameDataDir> <cacheDir> <House.iff> [ticks]\n"); return 2; }
    int ticks = argc > 4 ? atoi(argv[4]) : 1800;
    FSO::Content::Content::Init(argv[1], argv[2]);
    auto c = FSO::Content::Content::Get();
    printf("objects indexed: %zu\n", c->WorldObjects->Entries.size());
    VM::UseWorld(false);
    VMContext ctx(nullptr);
    Model::VMNullHeadlineProvider hp;
    VM vm(&ctx, &hp);
    vm.Init();
    int registered = 0; for (auto& p : ctx.Primitives) if (p) registered++;
    printf("primitives registered: %d\n", registered);
    std::map<std::string,int> exceptions; int dialogs = 0;
    vm.OnDialog = [&](Model::VMDialogInfo& i) {
        dialogs++;
        if (getenv("SIMRUN_VERBOSE") && dialogs <= 3) printf("---- %s\n%s\n", i.Title.c_str(), i.Message.c_str());
        if (i.Title == "SimAntics Exception!") {
            auto m = i.Message; auto p = m.find("\n\n"); std::string key = m.substr(p == std::string::npos ? 0 : p + 2, 80);
            exceptions[key]++;
        }
    };
    FSO::Files::Formats::IFF::IffFile house(argv[3]);
    VMWorldActivator act(&vm, nullptr);
    act.LoadFromIff(&house);
    printf("lot %dx%d, entities: %zu, rooms: %zu\n", ctx.Architecture()->Width, ctx.Architecture()->Height, vm.Entities.size(), ctx.RoomInfo.size());
    for (int i = 0; i < ticks; i++) vm.Update();
    int idle = 0, busy = 0;
    for (auto e : vm.Entities) if (e->Thread) { if (e->Thread->Stack.size() > 1 || !e->Thread->Queue.empty()) busy++; else idle++; }
    printf("after %d ticks: entities %zu (idle %d, busy %d), clock %02d:%02d, dialogs %d\n",
           ticks, vm.Entities.size(), idle, busy, ctx.Clock->Hours, ctx.Clock->Minutes, dialogs);
    int total = 0; for (auto& kv : exceptions) total += kv.second;
    printf("object resets (SimAntics exceptions): %d\n", total);
    for (auto& kv : exceptions) printf("  %3dx %s\n", kv.second, kv.first.c_str());
    return 0;
}
