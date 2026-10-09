#!/usr/bin/env python3
"""Converts a game's music from the original Nova Arcade (ESP32) format into Nova HD song text.
    python3 tools/port_music.py ../nova-arcade/games/Blockfall/game.h
Prints, for every song in the original, its tempo, chords, lead line and drum patterns in the format
of engine/audio.h. The melodies are the original game's own; pick instruments when pasting them in."""
import re
import sys

CHORDS = {"AM": "Am", "F_": "F", "C_": "C", "G_": "G", "E_": "E", "BB": "Bb", "DM": "Dm", "EM": "Em", "D_": "D", "A_": "A"}
NAMES = ["C", "C#", "D", "Eb", "E", "F", "F#", "G", "Ab", "A", "Bb", "B"]


def note(m):
    return "%s%d" % (NAMES[m % 12], m // 12 - 1)


def main(path):
    src = open(path).read()
    leads = []
    m = re.search(r"LEADS\[\]\[16\]\s*=\s*\{(.*?)\n\};", src, re.S)
    if m:
        for row in re.findall(r"\{([\d,\s]+)\}", m.group(1)):
            leads.append([int(v) for v in row.split(",") if v.strip()])
    drums = []
    m = re.search(r"DRUMS\[\]\s*=\s*\{(.*?)\};", src, re.S)
    if m:
        drums = re.findall(r'"([^"]*)"', m.group(1))
    bars = {}
    for name, body in re.findall(r"static const Bar (\w+)\[\]\s*=\s*\{(.*?)\};", src, re.S):
        bars[name] = [(c, int(l), int(d)) for c, l, d in re.findall(r"\{\s*(\w+)\s*,\s*(-?\d+)\s*,\s*(\d+)\s*\}", body)]
    m = re.search(r"SongDef SONGS\[\]\s*=\s*\{(.*?)\n\};", src, re.S)
    songs = re.findall(r"\{(\w+),\s*(\d+),\s*(\d+),\s*(true|false)", m.group(1)) if m else []
    for name, nb, bpm, loop in songs:
        if name == "nullptr":
            continue
        b = bars[name]
        chords = " ".join(CHORDS.get(c, c) for c, _, _ in b)
        lead_bars, drum_bars = [], []
        for c, li, di in b:
            if li < 0 or li >= len(leads):
                lead_bars.append(" ".join(["."] * 16))
            else:
                toks = []
                for v in leads[li]:
                    toks.append("." if v == 0 else "-" if v == 1 else note(v))
                lead_bars.append(" ".join(toks))
            drum_bars.append(drums[di] if di < len(drums) else "." * 16)
        print("// %s: %s bars, %s bpm, loop %s" % (name, nb, bpm, loop))
        print('  %s, %s, "%s",' % (bpm, nb, chords))
        print('  lead: "' + ' |"\n        "'.join(lead_bars) + '"')
        print('  drums: "' + "|".join(drum_bars) + '"')
        print()


if __name__ == "__main__":
    main(sys.argv[1])
