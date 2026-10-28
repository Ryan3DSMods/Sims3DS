# src/audio + src/hit — sound and music, "just like the original"

The Sims' sound is driven by **HIT**, Maxis' little sound VM: objects, animations and the UI fire *events*
(`toilet_flush`, `vox_flirter`, `ui_piemenu_appear`, `stereo_rock`...), a bank maps each event to a *track*
whose bytecode picks samples from *hitlists* (random variations, male / female / child voices) and plays
*patches* (Maxis XA ADPCM or WAV samples inside the FAR archives). This is a translation of FreeSims' HIT stack
(TSO-oriented) re-targeted at The Sims 1's banks, plus a small software mixer and two platform backends.

```
src/files/HIT/      FSO.Files.HIT: HITFile HSM Hot EVT TLO Patch Hitlist Track FSC HITConstants (+ HITText helpers)
src/files/XA/       XAFile   Maxis XA ADPCM -> PCM (all TS1 sound effects / voices / TV clips)
src/files/UTK/      UTKFile2 Maxis UTalk speech codec (TSO; translated for completeness)
src/content/Audio   FSO.Content.Audio: finds the TS1 banks, indexes the sound FARs, TV stations, music folders,
                    decodes samples on demand into a byte-budgeted LRU cache
src/hit/            TSO.HIT: HITVM HITThread HITSound HITInterpreter HITTVOn (radio/TV/mode music)
                    HITResourceGroup HITEventRegistration FSCPlayer AmbiencePlayer
src/audio/          AudioMixer (N voices, resampling, groups), SoundBuffer/AudioStream, Mp3Stream (minimp3),
                    WavFile (read/stream/write), AudioBackend (+ host WavCaptureBackend), GameAudio (the API)
src/platform/3ds/AudioNdsp   libctru NDSP backend (audio thread, double/quad-buffered wave buffers)
src/third_party/minimp3      minimp3 (CC0) for the MP3 music
src/tools/simaudio.cpp       host tool: decode, play events/music to WAV, sweep every event
```

## API for the app (other agents: this is all you need)

```cpp
#include "audio/GameAudio.h"
using namespace simaudio;

// platform (once, before App::Init):
GameAudio::Config c; c.DataRoot = root;          // install tree: SoundData/, Music/, ExpansionPack*/...
GameAudio::Get().Init(c);                        // false = no SoundData: every call below is a no-op
GameAudio::Get().SetBackend(std::make_unique<NdspBackend>());   // 3DS  (host: WavCaptureBackend("out.wav"))

GameAudio::Get().Update(dtMs);                   // every frame (App::Frame does it)
GameAudio::Get().Backend()->Pump(dtMs);          // host capture backend only (the 3DS backend has its own thread)

GameAudio::Get().PlayUI("ui_click");             // any UI event (see list below)
GameAudio::Get().SpeedChanged(1, 3);             // ui_speed_1to3 (0 = pause: ui_speed_3top ...)
GameAudio::Get().SetMusicMode(MusicMode::Buy);   // 3 s fade, then the next mode's songs, shuffled, back to back
GameAudio::Get().SetNeighbourhood(NeighbourhoodKind::MagicTown);  // Buy/Build/Nhood -> the pack's own score
GameAudio::Get().SetVolumes(sfx, music, voice);  // options sliders 0..1
GameAudio::Get().LeaveLot();                     // before destroying a lot's VM (stops radios, TVs, loops)
GameAudio::Get().SetSuspended(true);             // HOME menu / sleep
GameAudio::Get().PlayFile(cache + "/intro.wav"); // ADDED(sims): one-shot PCM WAV (preloaded, music group): the EA intro
GameAudio::Get().StopFile();                     //   ... stopped when the intro is skipped / ends (FrontEnd State::Intro)
GameAudio::Get().OutputLatencyMs();              //   queued output (3DS ~110 ms, host 0): the intro holds its video by it
```

`MusicMode`: `Live` (silence: TS1 has no live-mode score, only radios/TVs), `Buy`, `Build`, `Neighborhood`,
`Load` (loadloop.wav), `Family` (Create-A-Family), `Options`, `Credits`, `Fade`, and the pack variants
`Downtown`, `Vacation`, `OldTown`, `StudioTown`, `MagicTown`, `MagicTownBuild`, `MagicTownBuy`, `MagicTownCredits`.
Repeating the current mode is a no-op, so screens can call it every time they open.

UI event names (SimsGeneratedHitSource.hot): `ui_click ui_error ui_whoosh ui_help ui_piemenu_appear
ui_piemenu_highlight ui_piemenu_select ui_queued ui_queue_delete ui_buy_place ui_moneyback ui_remove
ui_object_place ui_object_move_place ui_object_rotate ui_object_moneyback ui_bld_dragtool_mousedown
ui_bld_dragtool_mouseup ui_bld_dragtool_place ui_terrain_raise ui_terrain_lower ui_terrain_level
ui_cac_cycleparts ui_cac_cyclehead ui_cac_personpts ui_camera_photo ui_nhood_click ui_nhood_rollover
ui_nhood_error ui_nhood_bdoze ui_nhood_bdoze_demolish ui_nhood_bdoze_evict ui_speed_1to2 ... ui_speed_pto3`.

Wired so far (minimal hooks; polish pass: CAS sounds in `cas/CASScreen.cpp` — ui_cac_cyclehead/cycleparts/personpts,
ui_click, ui_error when out of points; CAS opens with `bkground_family` = the neighbourhood tune; the lot's sounds
pause with the game and in buy/build (`App::AudioFrame`); a station a lot's controller objects start (Downtown
jazz...) becomes that lot's live-mode music; travel uses the loading screen's loadloop; options sliders reach
`SetVolumes` through front/GameOptions -> FrontVolumes. Dialog stings need no hook: in 78 of the 162 base-game
dialog trees the BHAV itself plays the sting via Play Sound, the rest are silent in the original too).
Lot UI (`// FIX(sims) SND`, FreeSims call sites, names in `app/UISounds.h`): buying an object = `ui_object_place`
(the cash register: same sample as `ui_buy_place`), putting a moved object down = `ui_object_move_place`, sell = `ui_moneyback`,
rotate = `ui_object_rotate`, can't place / can't afford / can't pick up = `ui_error`; walls = `ui_bld_dragtool_mousedown`
then `_place` (or `_mouseup` when nothing was built); wallpaper / floor / pool = `_mousedown` / `_mouseup`; terrain raise
/ lower = `ui_terrain_raise` / `_lower` per step, level = `ui_terrain_level_mousedown` / `_mouseup`, grass = drag tool;
every button (modes, categories, page arrows, tools, options, dialogs) = `ui_click` (FreeSims UIButton), catalogue items
silent. UI events are global HIT threads, so they play while the lot's sounds are paused (buy/build, speed 0).
Original hooks: `App::Frame` (Update, music by screen: chooser = Neighborhood, loading = Load,
lot = Live/Buy/Build by `LotScreen::CurrentMode`, speed clicks, pie-menu appear), `App::LeaveLot`,
`LotScreen` (ui_queued on a queued interaction, ui_queue_delete on cancel). The front end / CAS / buy / build
screens should call `PlayUI` / `SetMusicMode` themselves (FreeSims' call sites are listed in UISounds.cs).

## How a sound plays (all of this runs without the UI)

1. **SimAntics** `Play Sound` (primitive 23, `VMPlaySound`): the object's FWAV chunk names the event; the owner
   (caller or stack object) gets a `VMSoundEntry`; Sims also start events from animation `sound` time-props
   (`VMAvatar::HandleTimePropsEvent`) and pass their data (`SubmitHITVars`: gender / child) so the HIT code picks
   the right Simlish voice (`generic_fm_smartlists`). `Stop All Sounds` (48) releases them.
2. **HITVM::PlaySoundEvent**: event -> registration (highest LoadPriority bank wins: Hot Date's converse voices
   replace the base ones) -> `kSndobPlay`: a `HITThread` running the track's bytecode; `kTurnOnTV`: a `HITTVOn`
   (TV station or radio); `kSetMusicMode`: mode music; `kKillInstance` / `kSndobCancelNote`: act on the calling
   object's station (TV off, "next song").
3. **HITThread::Tick** (60 Hz): runs bytecode until it waits (`wait`, `wait_samp`); `note_on` decodes the patch
   (cache) and starts a mixer voice.
4. **VMEntity::TickSounds** (every VM tick): pan = screen x of the object, volume falls off with the distance from
   the view centre, the zoom level (far = quieter) and other floors — FreeSims' formula; looping sounds whose
   thread ended are restarted. Stings / UI (`kArgsNormal` tracks) stay centred.
5. **AudioMixer** mixes; the backend outputs (3DS: NDSP thread; host: WAV file).

Pausing the game pauses object sounds (VM::Update), as in TS1. Stings (`kDuckPriNever`: death, fire,
promotion...) duck the music group while they play.

### Stations (TS1 track id -> content)
| event | id | content |
|---|---|---|
| stereo_country* | 260 | Music/Stations/Country |
| stereo_classical* | 269 | Music/Stations/Classica |
| stereo_jazz* ("TrkBossaNova") | 270 | Music/Stations/Latin |
| stereo_rock* | 280 | Music/Stations/Rock |
| djbooth_countrydance / rap / rave / disco / beach | 6681-6685 | CountryD, Rap, Rave, Disco, Beach |
| stereo_zydeco, speakers_unl_cheap / _exp, stereo_unleashedui | 22900-22902 | UnlSpeak, UnlCheap |
| station_ezcheap / ezexp | 65536/65537 | EZ, EZExp |
| stereo_superstarspeaker | 100100 | Stations/Superstar |
| tv_romance/mystery/action/cartoon_* | 26-29 | SoundData.far TVStations/<channel>/<show>/*.xa + commercials |
| tv_superstar_animal/entertainment/news/sports | 103000-103003 | ExpansionPack6 Sound.far TVStations |

TV channels play one show (a lettered folder of clips) then a commercial, in a shuffled show order.

## Formats (what was found on the discs)
* Sound effects / voices / TV: **Maxis XA** ADPCM (`XAI\0`), 22050 Hz mono mostly, in `SoundData/SoundData.far`
  (2 904 XA + 43 WAV stings), `ExpansionShared/Sound/Sound.far`, `ExpansionPack5-7/Sound/Sound.far`,
  `ExpansionPack/ExpansionPack.far` and each `Downloads/*/*.far`.
* Banks: `.hot` (ini: `[EventMapping] [Track] [Patch] [GlobalHitList] [TrackData]`), `.hsm` (labels), `.hit`
  (bytecode). Branch/call operands are **TrackData label ids**, `patch` is variable 0x32 — see HITInterpreter.h.
* Music: **MP3** (MPEG-1 layer III 128 kbps 44.1 kHz stereo) in `Music/Modes/*` and `Music/Stations/*`;
  `Music/Modes/Load/loadloop.wav` (22 kHz stereo PCM, looped). The empty radio/TV hitlists in the banks (221-235,
  528-531) were filled by the original exe from these folders; `HITTVOn` does the same.

## 3DS notes
* Output 32728 Hz stereo s16, one NDSP channel, 4 x 1024-frame wave buffers (125 ms), mixed on an audio thread
  (New 3DS: core 2; else core 1 with a 30 % time limit) above the game thread's priority. Needs
  `sdmc:/3ds/dspfirm.cdc`.
* Memory: banks ~5 MB parsed (all packs), FAR index ~0.4 MB, sample cache 6 MB (LRU; a voice line is 30-200 KB
  decoded), MP3s stream through a 16 KB window + 4.5 KB frame — nothing scales with song length.
* CPU (callgrind instruction counts, x86 build with minimp3's SIMD off, as a proxy for the ARM11): 128 kbps MP3
  decode + cubic resample to 32728 Hz = 14 M instr/s; 8 simultaneous 22 kHz effect voices = 19 M instr/s. That is
  ~2-3 % of an 804 MHz core each, maybe 2-3x that with the ARM11's slower VFP. `NdspBackend::MixMs` reports the
  real cost per 1024-frame buffer on hardware.
* Quality: XA decode bit-exact vs ffmpeg; MP3 decode = ffmpeg within 100 dB; mixer resampling (Catmull-Rom, 32.32
  fixed-point position) 78 dB SINAD on a 1 kHz sine, 50 dB SNR vs an ideal polyphase resampler on real music.
* SD card layout: copy `SoundData/`, `Music/` and the `ExpansionPack*/`, `ExpansionShared/`, `Downloads/`
  folders next to `GameData/` (sdmc:/3ds/sims/...). Without them the game is silent but runs.
