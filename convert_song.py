#!/usr/bin/env python3
"""
Chuyển file nhạc dạng:   M<motor>,<tần số Hz>,<bắt đầu (s)>,<độ dài (s)>
thành song_data.h để nhúng vào firmware STM32.

Mỗi motor chỉ phát được MỘT nốt tại một thời điểm, nên script "đơn âm hóa"
từng motor: khi nhiều nốt chồng lấn, nốt BẮT ĐẦU SAU CÙNG được ưu tiên.
  - mặc định : khi nốt mới kết thúc, nốt cũ còn đang giữ sẽ phát tiếp (resume)
  - --no-resume : nốt mới cắt nốt cũ, hết nốt mới thì im lặng

Mỗi sự kiện được nén vào 32 bit:  [ thời điểm ms : 20 bit | tần số Hz : 12 bit ]
(tần số = 0 nghĩa là nghỉ/im lặng).

Cách dùng:
    python convert_song.py output__1_.txt -o ../Core/Inc/song_data.h
"""
import argparse
import os
import sys

N_MOTORS = 3
MAX_T_MS = (1 << 20) - 1      # ~17,4 phút
MAX_F_HZ = (1 << 12) - 1      # 4095 Hz


def parse(path):
    notes = [[] for _ in range(N_MOTORS)]
    with open(path, encoding="utf-8") as fh:
        for n, line in enumerate(fh, 1):
            line = line.strip()
            if not line or line.startswith("#"):
                continue
            try:
                m, f, t, d = line.split(",")
                m = int(m.strip().lstrip("Mm"))
                f = int(round(float(f)))
                t = float(t)
                d = float(d)
            except ValueError:
                sys.exit(f"Dòng {n} sai định dạng: {line!r}")
            if not 0 <= m < N_MOTORS:
                sys.exit(f"Dòng {n}: motor M{m} ngoài phạm vi M0..M{N_MOTORS - 1}")
            if not 0 < f <= MAX_F_HZ:
                sys.exit(f"Dòng {n}: tần số {f} Hz ngoài phạm vi 1..{MAX_F_HZ}")
            notes[m].append((round(t * 1000), round((t + d) * 1000), f))
    return notes


def monophonic(notes, resume, gap_ms):
    """notes: [(on_ms, off_ms, hz)] -> [(t_ms, hz)] (hz = 0 là im lặng)."""
    ev = []
    for i, (on, off, _f) in enumerate(notes):
        if gap_ms and off - on > 4 * gap_ms:
            off -= gap_ms                      # khoảng nghỉ nhỏ để tách các nốt lặp
        ev.append((on, 1, i))                  # 1 = bắt đầu
        ev.append((off, 0, i))                 # 0 = kết thúc (xử lý trước nốt mới cùng thời điểm)
    ev.sort()

    active, timeline = [], []
    for t, kind, i in ev:
        if kind:
            if resume:
                active.append(i)
            else:
                active = [i]
        elif i in active:
            active.remove(i)
        f = notes[active[-1]][2] if active else 0
        if timeline and timeline[-1][0] == t:
            timeline[-1] = (t, f)
        else:
            timeline.append((t, f))

    out, cur = [], 0                           # bỏ các sự kiện không đổi tần số
    for t, f in timeline:
        if f != cur:
            out.append((t, f))
            cur = f
    return out


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("input")
    ap.add_argument("-o", "--output", default="song_data.h")
    ap.add_argument("--no-resume", action="store_true",
                    help="nốt mới cắt hẳn nốt cũ (không phát tiếp nốt cũ)")
    ap.add_argument("--gap-ms", type=int, default=15,
                    help="rút ngắn cuối mỗi nốt để các nốt lặp tách bạch (mặc định 15 ms)")
    ap.add_argument("--lead-ms", type=int, default=500,
                    help="im lặng trước nốt đầu tiên (mặc định 500 ms)")
    args = ap.parse_args()

    notes = parse(args.input)
    tracks = [monophonic(n, not args.no_resume, args.gap_ms) for n in notes]

    t0 = min(tr[0][0] for tr in tracks if tr)
    shift = t0 - args.lead_ms
    tracks = [[(t - shift, f) for t, f in tr] for tr in tracks]
    length_ms = max(tr[-1][0] for tr in tracks if tr)
    if length_ms > MAX_T_MS:
        sys.exit(f"Bài dài {length_ms} ms, vượt giới hạn {MAX_T_MS} ms của định dạng 20 bit")

    total = sum(len(t) for t in tracks)
    with open(args.output, "w", encoding="utf-8") as o:
        o.write("/* Tự sinh bởi tools/convert_song.py - KHÔNG sửa tay.\n")
        o.write(f" * Nguồn : {os.path.basename(args.input)}\n")
        o.write(f" * Độ dài: {length_ms / 1000:.1f} s | {total} sự kiện "
                f"| {total * 4} byte flash\n")
        o.write(" * Chỉ #include file này trong stepper_player.c (mảng static const).\n */\n")
        o.write("#ifndef SONG_DATA_H\n#define SONG_DATA_H\n\n#include <stdint.h>\n\n")
        o.write("/* sự kiện = (thời điểm ms << 12) | tần số Hz ; tần số 0 = im lặng */\n")
        o.write("#define SONG_EV(t_ms, f_hz)  (((uint32_t)(t_ms) << 12) | (uint32_t)(f_hz))\n")
        o.write("#define SONG_TIME_MS(e)      ((uint32_t)(e) >> 12)\n")
        o.write("#define SONG_FREQ_HZ(e)      ((uint32_t)(e) & 0xFFFu)\n\n")
        o.write(f"#define SONG_LENGTH_MS       {length_ms}u\n\n")
        for m, tr in enumerate(tracks):
            o.write(f"/* Motor M{m}: {len(tr)} sự kiện */\n")
            o.write(f"static const uint32_t song_m{m}[] = {{\n")
            for k in range(0, len(tr), 6):
                chunk = tr[k:k + 6]
                o.write("  " + " ".join(f"SONG_EV({t},{f})," for t, f in chunk) + "\n")
            o.write("};\n\n")
        o.write("#endif /* SONG_DATA_H */\n")

    for m, tr in enumerate(tracks):
        print(f"M{m}: {len(notes[m])} nốt -> {len(tr)} sự kiện")
    print(f"Tổng {total} sự kiện = {total * 4} byte | dài {length_ms / 1000:.1f} s -> {args.output}")


if __name__ == "__main__":
    main()
