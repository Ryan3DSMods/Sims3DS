# Cheats

The Sims' cheat console, as in the PC Complete Collection (`src/app/ui/UICheatConsole.*`, marked `TS1(sims)`).

**Open it:** hold **L + R** and press **SELECT** (the PC's Ctrl+Shift+C) — in the neighbourhood and on a lot — or tap
**Cheat** at the top right of the lot's Options panel. The cheat box appears at the top of the top screen (the
prompt "Enter Cheat Code:", a text field and the last five lines entered/printed), then the console's system keyboard
(swkbd) opens. OK runs the line; Cancel closes the box. With the box open: **A** types another cheat, **B** (or the
chord again) closes it. L or R alone still rotate the lot / switch the neighbourhood (a shoulder press waits 0.1 s to see
whether it is the chord).

Like the original: cheats are separated by `;`, `!` repeats the previous cheat (`rosebud;!;!;!` = +§4,000, `!;!`
works too, and `!` alone repeats the last cheat of the previous line), unknown text does nothing and prints nothing,
and cheats stay on until the game is closed (they are not saved).

| cheat | what it does here |
|---|---|
| `rosebud`, `klapaucius` | +§1,000 to the family funds (lot budget, VM globals 13/14), capped at §9,999,999; the funds display counts up and the cash sound (`ui_moneyback`) plays. Saved with the lot into the family's money in Neighborhood.iff. In the neighbourhood there is no budget: no effect (as in TS1). |
| `set_hour <1-24>` | the clock jumps to that hour (24 = midnight), minutes 0 |
| `move_objects on/off` | buy/build hand picks up anything: objects the game locks (mailbox, outside trash can, objects in use) and **Sims**. A held Sim is put down on another tile (it is reset, its queue dropped) or "Sell" removes it from the lot; the family keeps it, so it comes back (motives refreshed) the next time the lot loads — the original's trick. Deleting the mailbox upsets NPCs that walk to it, as in the original. |
| `map_edit on/off` | build tools and placing/moving objects anywhere on the lot, outside the buildable area (street, sidewalk) |
| `tile_info on/off` | the tile under the cursor: position, story, room, floor pattern, wall segments, altitude, grass, "(locked)" outside the buildable area |
| `route_balloons on/off` | balloons on the waypoints of the routes the Sims are walking |
| `draw_routes on/off` | those routes as yellow lines |
| `sim_speed <-1000..1000>` | slower / faster simulation at every speed setting: 2^(n/250), i.e. 1/16x .. 16x (port mapping; 0 = normal) |
| `sweep on/off` | off freezes the simulation (no ticks) |
| `autonomy <1-100>` | the family Sims' autonomy level (person data 36, which the game's scripts read; 50 is the default the game gives every Sim) |
| `grow_grass <0-150>` | the whole lawn at that growth: 0 = dead brown, 150 = lush green (saved with the lot) |
| `interests` | prints the selected Sim's interests (0-10; children: Toys/Aliens/Pets/School) |
| `rotation <0-3>` | camera rotation |
| `house <n>` | in the neighbourhood: loads house n (when a family lives there) |
| `quit` | the game's Quit (with "Save?") |
| `water_tool` | accepted: in the Complete Collection the water tool is part of build mode already (Hot Date) |
| `preview_anims`, `draw_all_frames`, `draw_floorable` `on/off` | accepted (engine debug views of the PC renderer; no effect) |
| `edit_char`, `prepare_lot`, `history`, `hist_add`, `sim_log`, `log_mask`, `dump_happy`, `html`, `reload_people`, `cam_mode`, `music`, `sound`, `soundevent`, `draw_origins`, `debug_social`, `import`, `lot_size`, `visitor_control`, `auto_level`, `tutorial`, `genable_objects`, `nessie`, `xyzzy`, `crash`, `vsync` | accepted silently (authoring/debug cheats with nothing to act on in this port; `crash` does not crash) |

There are no other TS1 cheats; console-version codes (`midas`, `freeall`, `party m`, ...) and The Sims 2/3 codes
("motherlode", "make me a millionaire", bulldozer...) are not part of the PC game and are not added.
Host simulator (`build-port/sim3ds_host`, see src/app/README.md): `cheat TEXT` (chord + TEXT typed), `chord`, `budget`.

