"""Where a movie's PCM track sits in a recording of the engine's mix: finds the movie in the recording and the byte
offset of the PCM against the sound unit's 512-byte L / 512-byte R interleave. Offset 0 = played as authored; any other
value means the core input mixed the two channels block by block (a buzz; A01MP02 after A01MP05: 676).

usage: python -I tools/movie_align.py A01MP02 <raw.pcm> [--ext PSW] [--seconds 6,12,20]
<raw.pcm>: GOW_AUDIO_RAW recording (s16le stereo 48 kHz, before the output's rate adaptation: sample-exact).
--ext: the file the game played (.PSS 4:3, .PSW 16:9; their PCM is the same bytes, the packets differ).
"""
import argparse
import os
import struct

import numpy as np

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
DISC = os.path.join(ROOT, 'disc-usa')
RATE = 48000


def read_movie(name):
    toc = open(os.path.join(DISC, 'GODOFWAR.TOC'), 'rb').read()
    for i in range(0, len(toc) - 23, 24):
        if toc[i:i + 12].split(b'\0')[0].decode('ascii', 'replace').upper() == name.upper():
            size, sector = struct.unpack_from('<II', toc, i + 16)
            for part in (1, 2):
                with open(os.path.join(DISC, f'PART{part}.PAK'), 'rb') as f:
                    f.seek(sector * 2048)
                    data = f.read(size)
                if data[:4] == b'\0\0\1\xba':
                    return data
    raise SystemExit(f'{name} not found in GODOFWAR.TOC / PART*.PAK')


def pcm_track(data):
    """The PCM bytes of private stream 1 (sub-stream 0xA0) without the 40-byte SShd/SSbd header."""
    pos, parts = 0, []
    while pos + 6 <= len(data):
        code = data[pos:pos + 4]
        if code == b'\0\0\1\xba':
            pos += 14 + (data[pos + 13] & 7)
            continue
        if code[:3] != b'\0\0\1' or code[3] == 0xB9:
            break
        length = struct.unpack('>H', data[pos + 4:pos + 6])[0]
        if code[3] == 0xBD:
            payload = data[pos + 9 + data[pos + 8]:pos + 6 + length]
            if payload[1] == 0xA0:
                parts.append(payload[4:])
        pos += 6 + length
    track = b''.join(parts)[40:]
    return np.frombuffer(track[:len(track) // 2 * 2], dtype='<i2').astype(np.float32)


def reader(track, offset, frames):
    """What the core input plays when the ring holds the track shifted by `offset` bytes: 256 L then 256 R samples."""
    blocks = min(frames // 256, (len(track) - offset // 2) // 512)
    out = np.zeros((blocks * 256, 2), np.float32)
    for k in range(blocks):
        b = offset // 2 + 512 * k
        out[k * 256:(k + 1) * 256, 0] = track[b:b + 256]
        out[k * 256:(k + 1) * 256, 1] = track[b + 256:b + 512]
    return out


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('movie')
    ap.add_argument('raw')
    ap.add_argument('--ext', default='PSS')
    ap.add_argument('--seconds', default='6,12,20', help='where in the movie to measure')
    args = ap.parse_args()
    track = pcm_track(read_movie(f'{args.movie}.{args.ext}'))
    raw = np.fromfile(args.raw, dtype='<i2').reshape(-1, 2).astype(np.float32)
    seconds = [int(s) for s in args.seconds.split(',')]
    need = (max(seconds) + 1) * RATE
    # coarse: where the movie (as authored) lies in the recording, from a 2 s excerpt of the mono sum
    ref = reader(track, 0, need)
    a, b = raw[:, 0] + raw[:, 1], ref[5 * RATE:7 * RATE, 0] + ref[5 * RATE:7 * RATE, 1]
    n = 1 << int(np.ceil(np.log2(len(a) + len(b))))
    start = int(np.argmax(np.fft.irfft(np.fft.rfft(a, n) * np.conj(np.fft.rfft(b, n)), n)[:len(a)])) - 5 * RATE
    print(f'{args.movie}.{args.ext}: movie starts near recording sample {start} ({start / RATE:.2f} s)')
    shifted = {d: reader(track, d, need)[:, 0] for d in range(0, 1024, 2)}
    for s in seconds:
        j, best = s * RATE, (0.0, 0)
        window = raw[start + j - 3000:start + j + 3000 + 4096, 0]
        for d, r in shifted.items():
            y = r[j:j + 4096]
            if len(y) < 4096:
                continue
            t = int(np.argmax(np.correlate(window, y, 'valid')))
            x = window[t:t + 4096]
            c = float(np.dot(x, y) / np.sqrt(np.dot(x, x) * np.dot(y, y) + 1))
            best = max(best, (c, d))
        print(f'  {s:3d} s: offset {best[1]} bytes, correlation {best[0]:.4f}')


if __name__ == '__main__':
    main()
