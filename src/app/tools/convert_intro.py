#!/usr/bin/env python3
# This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of the MPL was not
# distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
"""Converts the game's EA intro (Music/eag_us.avi, MS-MPEG4v1 + PCM - not decodable on the 3DS) into
<cache>/intro.stv (the picture) and <cache>/intro.wav (the sound), played by the front end before the loading
screen (src/app/front/IntroScreen.h, FrontEnd State::Intro; the sound through GameAudio::PlayFile).

    python3 convert_intro.py <install>/Music/eag_us.avi <sd>/3ds/sims/cache/intro.stv [<intro.wav>]

The WAV goes next to the .stv (same name, .wav) unless given. Needs ffmpeg on the PC.
STV format: "STV1", u16 width, u16 height, u16 fps*100, u16 frames, then per frame u32 size + RLE pairs
(u8 count 1..255, u8 grey). The intro is a grey logo on black, so it packs well.
WAV: 16-bit stereo PCM at the 3DS mixer's output rate (32728 Hz: played without resampling), exactly as long as
the video (trimmed or padded with silence, a 10 ms fade where it is cut).
"""
import os, struct, subprocess, sys, wave

W, H, FPS = 400, 240, 30
RATE = 32728   # ADDED(sims): AudioMixer / NDSP output rate (src/audio/README.md "3DS notes")

def convert_video(src, dst):
    raw = subprocess.run(["ffmpeg", "-loglevel", "error", "-i", src, "-vf",
                          f"scale={W}:{H}:force_original_aspect_ratio=decrease,pad={W}:{H}:(ow-iw)/2:(oh-ih)/2,fps={FPS}",
                          "-f", "rawvideo", "-pix_fmt", "gray", "-"], check=True, stdout=subprocess.PIPE).stdout
    n = len(raw) // (W * H)
    out = bytearray(b"STV1" + struct.pack("<HHHH", W, H, FPS * 100, n))
    for f in range(n):
        px = raw[f * W * H:(f + 1) * W * H]
        rle = bytearray()
        i = 0
        while i < len(px):
            v = px[i] & 0xFC if px[i] < 8 else px[i]   # crush near-black noise
            j = i + 1
            while j < len(px) and j - i < 255 and ((px[j] & 0xFC if px[j] < 8 else px[j]) == v):
                j += 1
            rle += bytes((j - i, v))
            i = j
        out += struct.pack("<I", len(rle)) + rle
    open(dst, "wb").write(out)
    print(f"{n} frames, {len(out) // 1024} KB -> {dst}")
    return n

# ADDED(sims): the intro's sound track, which the 3DS port used to drop.
def convert_audio(src, dst, frames):
    def decode(resampler):
        af = ["-af", f"aresample={RATE}:resampler=soxr:precision=28"] if resampler else ["-ar", str(RATE)]
        return subprocess.run(["ffmpeg", "-loglevel", "error", "-i", src, "-vn", "-ac", "2"] + af +
                              ["-f", "s16le", "-acodec", "pcm_s16le", "-"], check=True, stdout=subprocess.PIPE).stdout
    try:
        pcm = decode(True)                 # SoX resampler (best quality) when ffmpeg has it
    except subprocess.CalledProcessError:
        pcm = decode(False)
    if not pcm:
        print(f"no audio track in {src}: {dst} not written (the intro plays silently)")
        return
    want = round(frames * RATE / FPS)      # sample frames = the video's length
    have = len(pcm) // 4
    samples = list(struct.unpack(f"<{have * 2}h", pcm[:have * 4]))
    if have > want:
        samples = samples[:want * 2]
        fade = min(want, RATE // 100)      # 10 ms fade-out where the sound is cut
        for k in range(fade):
            g = (fade - 1 - k) / fade
            i = (want - fade + k) * 2
            samples[i] = int(samples[i] * g); samples[i + 1] = int(samples[i + 1] * g)
    else:
        samples += [0] * ((want - have) * 2)
    with wave.open(dst, "wb") as w:
        w.setnchannels(2); w.setsampwidth(2); w.setframerate(RATE)
        w.writeframes(struct.pack(f"<{len(samples)}h", *samples))
    print(f"{want / RATE:.2f} s of sound ({RATE} Hz s16 stereo, source {have / RATE:.2f} s), "
          f"{os.path.getsize(dst) // 1024} KB -> {dst}")

def main():
    if len(sys.argv) < 3:
        print(__doc__); sys.exit(2)
    frames = convert_video(sys.argv[1], sys.argv[2])
    wav = sys.argv[3] if len(sys.argv) > 3 else os.path.splitext(sys.argv[2])[0] + ".wav"
    convert_audio(sys.argv[1], wav, frames)

if __name__ == "__main__":
    main()
