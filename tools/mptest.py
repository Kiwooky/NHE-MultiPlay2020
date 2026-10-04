#!/usr/bin/env python3
"""Audio tests for MultiPlay via lv2host."""
import numpy as np, subprocess, os, sys, tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
SO = os.environ.get('MP_SO', os.path.join(HERE, '..', 'bin', 'nhe-multiplay.lv2', 'nhe-multiplay_dsp.so'))
HOST = os.environ.get('MP_HOST', os.path.join(HERE, 'lv2host')).split()
ORDER = ['delay_time', 'width', 'speed', 'regen', 'mix', 'range', 'slam_level',
         'time_mod', 'hold', 'slam', 'ramp_time', 'lv2_enabled']
DEF = dict(delay_time=6, width=0, speed=3, regen=4, mix=5, range=2, slam_level=7,
           time_mod=0, hold=0, slam=0, ramp_time=0.45, lv2_enabled=1)
PORT = {s: 3 + i for i, s in enumerate(ORDER)}

def run(x, sr=48000, events=(), **kw):
    p = dict(DEF); p.update(kw)
    with tempfile.TemporaryDirectory() as td:
        fi, fo = os.path.join(td, 'i.raw'), os.path.join(td, 'o.raw')
        x.astype(np.float32).tofile(fi)
        args = HOST + [SO, fi, fo, str(sr), '1', '2'] + [str(p[s]) for s in ORDER]
        args += ['@%d:%d=%g' % (t, PORT[s], v) for (t, s, v) in events]
        subprocess.run(args, check=True)
        y = np.fromfile(fo, dtype=np.float32).reshape(-1, 2)
    return y[:, 0], y[:, 1]

def burst(sr, n, at=0.01, dur=0.004, f=1000, amp=0.25):
    x = np.zeros(n); i0 = int(at * sr); m = int(dur * sr)
    t = np.arange(m) / sr
    x[i0:i0 + m] = amp * np.sin(2 * np.pi * f * t) * np.hanning(m)
    return x

def onset(w, sr, start, thr):
    idx = np.where(np.abs(w[start:]) > thr)[0]
    return (start + idx[0]) / sr if len(idx) else None

fails = []
def check(name, ok, info=''):
    print(('PASS ' if ok else 'FAIL ') + name + ('  ' + info if info else ''))
    if not ok: fails.append(name)

def finite(*a):
    return all(np.all(np.isfinite(v)) for v in a)

def main():
    sr = 48000
    # 1. delay time per range, knob extremes, time mod -------------------
    exp = {2: 2.0, 1: 0.25, 0: 512 / 32768}
    rs = np.random.RandomState(7)
    for rng in (2, 1, 0):
        for tm in (0, 1):
            for k in (10, 0):
                full = exp[rng] * (2 if tm else 1)
                want = full if k == 10 else full / 13
                n = int((want * 1.5 + 0.3) * sr)
                x = np.zeros(n); i0 = int(0.05 * sr); m = int(0.03 * sr)
                x[i0:i0 + m] = 0.2 * rs.randn(m)
                o1, o2 = run(x, sr, delay_time=k, regen=0, range=rng, time_mod=tm)
                wet = o1 - x
                L = int(want * 1.5 * sr) + 10
                xc = [np.dot(wet[l:l + i0 + m], x[:i0 + m]) for l in range(0, L)] if L < 3000 else None
                if xc is None:
                    X = np.fft.rfft(x, 2 * n); W = np.fft.rfft(wet, 2 * n)
                    cc = np.fft.irfft(W * np.conj(X))[:L]
                else:
                    cc = np.array(xc)
                got = np.argmax(cc) / sr
                ok = abs(got - want) < max(0.02 * want, 0.00015)
                check('delay range=%d tm=%d knob=%d' % (rng, tm, k), ok,
                      'want %.4f got %.4f' % (want, got))

    # 2. level: tone in at -12 dBFS, wet level after one pass (regen 0) ----
    n = int(1.2 * sr); t = np.arange(n) / sr
    x = 0.25 * np.sin(2 * np.pi * 440 * t)
    o1, o2 = run(x, sr, delay_time=0, regen=0, range=1, mix=10)
    seg = slice(int(0.6 * sr), int(1.1 * sr))
    rin = np.sqrt(np.mean(x[seg] ** 2)); rw = np.sqrt(np.mean(o2[seg] ** 2))
    check('wet level ~ unity (out2 mix 10)', abs(20 * np.log10(rw / rin)) < 1.5,
          '%.2f dB' % (20 * np.log10(rw / rin)))
    r1 = np.sqrt(np.mean(o1[seg] ** 2))
    check('out1 mix 10 = wet only, ~unity', abs(20 * np.log10(r1 / rin)) < 1.5, '%.2f dB' % (20 * np.log10(r1 / rin)))
    o1d, o2d = run(x, sr, delay_time=0, regen=0, range=1, mix=0)
    check('out1 mix 0 = dry', np.max(np.abs(o1d - x)) < 1e-6, 'max err %.2e' % np.max(np.abs(o1d - x)))
    check('out2 mix 0 = dry', np.max(np.abs(o2d - x)) < 1e-6, 'max err %.2e' % np.max(np.abs(o2d - x)))
    # noise floor of the wet (8-bit + compander) relative to the tone
    from numpy.fft import rfft
    w = o2[seg] * np.hanning(seg.stop - seg.start)
    S = np.abs(rfft(w)) ** 2; f = np.fft.rfftfreq(len(w), 1 / sr)
    sig = S[(f > 430) & (f < 450)].sum(); noise = S[(f > 600) & (f < 18000)].sum()
    print('     wet SNR (440 Hz, -12 dBFS): %.1f dB' % (10 * np.log10(sig / noise)))

    # 3. regen decay, regen 10 sustain, slam oscillation bounded ------------
    n = int(6 * sr)
    x = burst(sr, n, amp=0.25, dur=0.02, f=500)
    for rg in (5, 10):
        o1, o2 = run(x, sr, delay_time=0, regen=rg, range=1)
        pk = [np.max(np.abs(o1[int(a * sr):int((a + 0.5) * sr)] - x[int(a * sr):int((a + 0.5) * sr)])) for a in (0.5, 2.0, 5.0)]
        check('regen %d finite/bounded' % rg, finite(o1, o2) and max(pk) < 2.0, 'peaks %s' % np.round(pk, 3))
    x = burst(sr, n, amp=0.1, dur=0.02, f=500)
    o1, o2 = run(x, sr, delay_time=0, regen=3, slam_level=10, range=1,
                 events=[(int(0.3 * sr), 'slam', 1), (int(4.0 * sr), 'slam', 0)])
    pk_s = np.max(np.abs(o1[int(2.5 * sr):int(4.0 * sr)]))
    pk_after = np.max(np.abs(o1[int(5.5 * sr):]))
    check('slam builds to oscillation, bounded', finite(o1, o2) and 0.3 < pk_s < 1.3, 'peak %.3f' % pk_s)
    check('slam release decays', pk_after < pk_s * 0.5, 'after %.3f' % pk_after)
    o1, o2 = run(x, sr, delay_time=0, regen=3, slam_level=4, range=1,
                 events=[(int(0.3 * sr), 'slam', 1)])
    pk4 = np.max(np.abs(o1[int(4.5 * sr):]))
    print('     slam level 4 after 4 s: peak %.3f' % pk4)

    # 4. hold: loop keeps playing with no input; knob varispeeds -------------
    n = int(3.0 * sr); t = np.arange(n) / sr
    x = 0.25 * np.sin(2 * np.pi * 1000 * t) * (t < 0.5)
    o1, o2 = run(x, sr, delay_time=0, regen=0, range=1, mix=10,
                 events=[(int(0.4 * sr), 'hold', 1), (int(1.8 * sr), 'delay_time', 10)])
    a = o2[int(1.0 * sr):int(1.6 * sr)]; b = o2[int(2.3 * sr):int(2.9 * sr)]
    def fpk(s):
        S = np.abs(np.fft.rfft(s * np.hanning(len(s)))); return np.fft.rfftfreq(len(s), 1 / sr)[np.argmax(S)]
    fa, fb = fpk(a), fpk(b)
    check('hold sustains loop', np.sqrt(np.mean(a ** 2)) > 0.1, 'rms %.3f' % np.sqrt(np.mean(a ** 2)))
    check('hold + delay knob varispeeds 13:1', abs(fa / fb - 13) < 1.0, 'f %.0f -> %.0f Hz' % (fa, fb))

    # 5. bypass: wet muted without clicks, dry exact, delay keeps running ----
    n = int(3 * sr); t = np.arange(n) / sr
    x = 0.25 * np.sin(2 * np.pi * 220 * t)
    o1, o2 = run(x, sr, regen=5, range=1,
                 events=[(int(1.0 * sr), 'lv2_enabled', 0), (int(2.0 * sr), 'lv2_enabled', 1)])
    seg = slice(int(1.6 * sr), int(1.9 * sr))
    d1 = np.max(np.abs(np.diff(o1))); dx = np.max(np.abs(np.diff(x)))
    check('bypass no clicks', d1 < 4 * dx, 'max step %.4f vs %.4f' % (d1, dx))
    check('bypass dry exact', np.max(np.abs(o1[seg] - x[seg])) < 1e-6,
          'err %.2e' % np.max(np.abs(o1[seg] - x[seg])))
    # bypass with MIX fully wet still passes the dry at unity on both outputs
    o1, o2 = run(x, sr, regen=5, range=1, mix=10, events=[(int(1.0 * sr), 'lv2_enabled', 0)])
    seg = slice(int(1.5 * sr), int(2.9 * sr))
    e = max(np.max(np.abs(o1[seg] - x[seg])), np.max(np.abs(o2[seg] - x[seg])))
    check('bypass at mix 10 = dry on both outputs', e < 1e-6, 'err %.2e' % e)
    d1 = np.max(np.abs(np.diff(o1)))
    check('bypass at mix 10 no clicks', d1 < 4 * dx, 'max step %.4f' % d1)
    # held loop survives bypass: record, hold, bypass, stop playing, re-engage
    n = int(4 * sr); t = np.arange(n) / sr
    x = 0.25 * np.sin(2 * np.pi * 700 * t) * (t < 0.8)
    o1, o2 = run(x, sr, delay_time=10, regen=0, range=1, mix=10,
                 events=[(int(0.6 * sr), 'hold', 1), (int(1.0 * sr), 'lv2_enabled', 0),
                         (int(2.5 * sr), 'lv2_enabled', 1)])
    off = np.sqrt(np.mean(o2[int(1.5 * sr):int(2.4 * sr)] ** 2))
    back = np.sqrt(np.mean(o2[int(2.8 * sr):int(3.8 * sr)] ** 2))
    check('held loop silent while bypassed', off < 1e-4, 'rms %.2e' % off)
    check('held loop returns on re-engage', back > 0.1, 'rms %.3f' % back)

    # 6. range switching and time mod mid-run --------------------------------
    o1, o2 = run(x, sr, regen=6, events=[(int(0.5 * sr), 'range', 0), (int(1.0 * sr), 'range', 1),
                                          (int(1.5 * sr), 'time_mod', 1), (int(2.0 * sr), 'range', 2)])
    check('range/time-mod switching finite', finite(o1, o2) and np.max(np.abs(o1)) < 2.5,
          'peak %.3f' % np.max(np.abs(o1)))

    # 7. torture, sample rates ----------------------------------------------
    for s in (44100, 48000, 96000):
        n = int(4 * s)
        x = (np.random.RandomState(1).randn(n) * 0.5).clip(-1, 1)
        o1, o2 = run(x, s, delay_time=10, width=10, speed=10, regen=10, mix=10, range=0,
                     slam_level=10, slam=1, time_mod=1)
        check('torture @%d' % s, finite(o1, o2) and np.max(np.abs(o1)) < 3.0, 'peak %.3f' % np.max(np.abs(o1)))
        n = int(1.5 * s); x = burst(s, n)
        o1, _ = run(x, s, delay_time=10, regen=0, range=1)
        t1 = onset(o1 - x, s, 0, 0.02); t0 = onset(x, s, 0, 0.02)
        check('delay accuracy @%d' % s, t1 and abs((t1 - t0) - 0.25) < 0.01, '%.4f s' % (t1 - t0))
    # silence in -> output quiet, no denormal stall
    x = np.zeros(int(2 * sr))
    o1, o2 = run(x, sr, regen=10)
    check('silence stays quiet', np.max(np.abs(o1)) < 1e-3, 'peak %.2e' % np.max(np.abs(o1)))

    print('\n%d failures' % len(fails), fails)
    return 1 if fails else 0

if __name__ == '__main__':
    sys.exit(main())
