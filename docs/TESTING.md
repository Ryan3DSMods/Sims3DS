# Testing

Summary of how releases are tested (from the developers' QA passes, October 2026). The test scripts, logs and
screenshots themselves are not in the repository because they are made from the game data.

## On real hardware

Played on a New 3DS (CIA build, 124 MB mode) by the author: boot, every neighbourhood, Create-A-Family, moving in,
entering houses and playing, Buy and Build mode, saving and reloading (after the save-safety rework), travel screens,
the HOME button / sleep, and the 3D HOME Menu banner. Hardware findings that were fixed: too many open files
(file-handle pool), a slow first Create-A-Sim/lot entry (directory scans → caches), the 178 MB memory mode crashing the
HOME Menu (now 124 MB), interrupted saves (atomic save + recovery), a pet-only family in the family bin.

Not yet exercised on the console: every expansion town feature in depth, very long sessions on one lot, the Old 3DS.

## On the PC build of the same code (`Makefile.port`)

Each release candidate is checked from clean builds, an empty save area and an empty cache:

| Area | Result |
|---|---|
| First start (save-area copy, object indexing, all 6 Unleashed families installed), later starts from the cache | pass |
| Neighbourhoods 1 and 2, Downtown, Vacation Island, Studio Town, Magic Town, Old Town maps | pass |
| Create-A-Family (adults, child, traits, heads/bodies), move in, "Purchase Lot?" | pass |
| Build mode (walls, floors, wallpaper, doors, windows, terrain, pools, roofs, costs) | pass |
| Buy mode: 885 of 887 residential catalogue entries can be bought and placed; rotate, move, sell with depreciation | pass (2 items delete themselves off community lots, by the data) |
| Live mode 3+ game days: skills, jobs, car pool, school bus, socials, visitors, fires and the fire brigade | pass |
| Save → quit → restart → reload: clock, budget, motives, skills, jobs, relationships kept | pass |
| Travel by phone: Downtown (date, kiss, cab home), Vacation (check-in, hot tub), Magic Town, Studio Town (15/15 phone round trips over 5 towns × 3 seeds), Old Town | pass |
| 69 expansion template lots loaded and run: 0 SimAntics resets, 0 unimplemented primitives | pass |
| Every original family house simulated for 2 game days: 0 resets | pass |
| Original houses saved back unchanged (byte-for-byte for unmodified houses), neighbourhood write-back, family import (.FAM) | pass |
| Sound events, mode music, radio stations | pass |
| Memory (32-bit build, same pointer sizes as the 3DS): peak 55–60 MB over long mixed sessions | pass |
| Every source file compiled with the 3DS CPU flags (`tools/arm_check/arm_check.sh`) | pass |
| Save safety: a scenario cut at every step (power-cut simulation) and started again — no lost or broken saves | pass |

Known open issues are listed in the README ("Status and known limitations").

## Repeat it yourself

```sh
make -f Makefile.port -j4 build-port/sim3ds_host build-port/simep build-port/simlife build-port/simsave
export SIM3DS_SAVEROOT=/tmp/sims_saves                      # keep tests out of your game folder
build-port/sim3ds_host <game folder> /tmp/sims_cache /tmp/shots src/app/tools/demo_script.txt
build-port/simep ...                                          # expansion lots sweep, see src/tools/simep.cpp
build-port/simlife <game folder> /tmp/sims_cache 5 2          # house 5 for 2 game days
tools/arm_check/arm_check.sh <libctru>/libctru/include
```
