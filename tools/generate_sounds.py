#!/usr/bin/env python3
"""ドリフトの効果音を合成して assets/sound/ に WAV で書き出す。

    python3 tools/generate_sounds.py

音はすべて 44100 Hz・16 bit・モノラル。ループする音は、終わりと始まりがつながるように作る。
ゲーム側（adapters/audio）は、ループ音の再生速度を変えて音の高さを変える。
必要なもの: numpy
"""

import os
import wave

import numpy as np

RATE = 44100
OUT_DIR = os.path.join(os.path.dirname(__file__), "..", "assets", "sound")
rng = np.random.default_rng(20261029)


def seconds(t):
    return int(round(t * RATE))


def timeline(t):
    return np.arange(seconds(t)) / RATE


def bandpass(signal, low, high):
    """FFT で low〜high Hz だけを残す。端はなめらかに落とす。"""
    spectrum = np.fft.rfft(signal)
    freqs = np.fft.rfftfreq(len(signal), 1.0 / RATE)
    gain = np.clip((freqs - low * 0.7) / (low * 0.3 + 1e-9), 0, 1) * np.clip((high * 1.3 - freqs) / (high * 0.3), 0, 1)
    return np.fft.irfft(spectrum * gain, len(signal))


def envelope(t, attack, decay):
    """立ち上がり attack 秒、そのあと decay 秒で指数的に減衰する。"""
    env = np.exp(-np.maximum(t - attack, 0) / decay)
    return env * np.clip(t / max(attack, 1e-6), 0, 1)


def make_loop(signal, fade):
    """終わりの fade 秒を始まりに重ねて、つなぎ目が聞こえないループにする。"""
    n = seconds(fade)
    head = signal[:n]
    tail = signal[-n:]
    ramp = np.linspace(0, 1, n)
    looped = signal[n:-n].copy() if len(signal) > 2 * n else signal[n:].copy()
    blend = tail * (1 - ramp) + head * ramp
    return np.concatenate([blend, looped])


def normalize(signal, peak=0.89):
    return signal * (peak / (np.max(np.abs(signal)) + 1e-9))


def fade_out(signal, duration=0.02):
    """最後の duration 秒を 0 へ下げる。鳴り終わりのプツッという音を防ぐ。"""
    n = seconds(duration)
    signal = signal.copy()
    signal[-n:] *= np.linspace(1, 0, n)
    return signal


def write(name, signal):
    os.makedirs(OUT_DIR, exist_ok=True)
    path = os.path.join(OUT_DIR, name)
    data = (np.clip(signal, -1, 1) * 32767).astype("<i2")
    with wave.open(path, "wb") as f:
        f.setnchannels(1)
        f.setsampwidth(2)
        f.setframerate(RATE)
        f.writeframes(data.tobytes())
    print(f"{path}: {len(signal) / RATE:.2f} s")


def engine():
    """エンジン。基本は 50 Hz（4 気筒の 1500 rpm）。ゲーム側で再生速度を変えて回転数を表す。

    1 秒ちょうどで 50 周期なので、倍音も含めて終わりと始まりが自然につながる。
    """
    t = timeline(1.0)
    base = 50.0
    sound = np.zeros_like(t)
    for n in range(1, 28):
        # 偶数次（点火の周期）を強め、高い倍音ほど弱くする。位相をずらしてざらつきを出す
        amp = (1.0 / n**0.85) * (1.6 if n % 2 == 0 else 0.7)
        sound += amp * np.sin(2 * np.pi * base * n * t + rng.uniform(0, 2 * np.pi))
    # 点火ごとの小さなうなり（1 周期に 2 回）
    sound *= 1.0 + 0.25 * np.sin(2 * np.pi * base * 2 * t)
    # 吸気と排気の荒さ。周期的なノイズにしてループさせる
    grit = bandpass(rng.normal(0, 1, len(t)), 300, 2400)
    grit *= 0.5 + 0.5 * np.sin(2 * np.pi * base * 2 * t) ** 2
    sound += 0.22 * grit / np.std(grit)
    sound = np.tanh(sound * 0.6)
    write("engine_loop.wav", normalize(sound, 0.8))


def squeal():
    """タイヤのスキール音。ゆらぐ高い音と、ザーッという擦れる音を重ねる。"""
    duration = 2.6
    t = timeline(duration)
    sound = np.zeros_like(t)
    # ゆっくりゆらぐ音程。タイヤが路面に引っかかっては離れる感じ
    wobble = np.cumsum(rng.normal(0, 1, len(t)))
    wobble = bandpass(wobble, 1, 9)
    wobble /= np.max(np.abs(wobble)) + 1e-9
    for freq, amp in [(820, 1.0), (1240, 0.55), (1650, 0.35), (2470, 0.18)]:
        phase = 2 * np.pi * np.cumsum(freq * (1 + 0.035 * wobble)) / RATE
        flutter = 0.7 + 0.3 * np.sin(2 * np.pi * rng.uniform(13, 21) * t + rng.uniform(0, 6))
        sound += amp * np.sin(phase) * flutter
    hiss = bandpass(rng.normal(0, 1, len(t)), 900, 5200)
    sound = 0.75 * sound / np.std(sound) + 0.6 * hiss / np.std(hiss)
    sound = np.tanh(sound * 0.45)
    write("squeal_loop.wav", normalize(make_loop(sound, 0.3), 0.8))


def whoosh():
    """ドリフトに入った瞬間の、空気を切る音。高い方へ抜けるノイズ。"""
    t = timeline(0.55)
    noise = rng.normal(0, 1, len(t))
    low = bandpass(noise, 200, 1200)
    high = bandpass(noise, 1500, 7000)
    sweep = np.clip(t / 0.4, 0, 1)
    sound = low * (1 - sweep) + high * sweep
    sound *= envelope(t, 0.06, 0.16)
    write("drift_start.wav", fade_out(normalize(sound, 0.7)))


def bell(t, freq):
    """澄んだ鐘の音。整数倍でない倍音を少し混ぜてきらきらさせる。"""
    sound = np.sin(2 * np.pi * freq * t)
    sound += 0.45 * np.sin(2 * np.pi * freq * 2.0 * t) * np.exp(-t / 0.12)
    sound += 0.25 * np.sin(2 * np.pi * freq * 3.01 * t) * np.exp(-t / 0.08)
    sound += 0.12 * np.sin(2 * np.pi * freq * 4.2 * t) * np.exp(-t / 0.05)
    return sound


def combo():
    """倍率が上がったときのチャイム。ゲーム側で倍率に合わせて音を高くする。"""
    t = timeline(0.7)
    sound = bell(t, 880) * envelope(t, 0.004, 0.22)
    # 少し遅れて 5 度上を重ね、上がっていく感じを出す
    late = np.clip(t - 0.06, 0, None)
    sound += 0.6 * bell(late, 1318.5) * envelope(late, 0.004, 0.2) * (t > 0.06)
    write("combo_up.wav", fade_out(normalize(sound, 0.75)))


def bank():
    """得点が確定したときのジングル。ドミソドと駆け上がって和音で鳴らす。"""
    t = timeline(1.4)
    sound = np.zeros_like(t)
    notes = [523.25, 659.25, 783.99, 1046.5]
    for i, freq in enumerate(notes):
        start = i * 0.07
        local = np.clip(t - start, 0, None)
        gate = t >= start
        decay = 0.5 if i == len(notes) - 1 else 0.25
        sound += bell(local, freq) * envelope(local, 0.004, decay) * gate
    # 最後に高いきらめき
    sparkle_t = np.clip(t - 0.28, 0, None)
    sparkle = bandpass(rng.normal(0, 1, len(t)), 5000, 11000) * envelope(sparkle_t, 0.01, 0.25) * (t > 0.28)
    sound += 0.25 * sparkle / (np.std(sparkle) + 1e-9) * 0.3
    write("drift_bank.wav", fade_out(normalize(sound, 0.8)))


def crash():
    """壁に当たった音。低いドンと、金属が鳴る音と、ざらついた擦れ。"""
    t = timeline(0.9)
    thud = np.sin(2 * np.pi * 55 * t * (1 - 0.3 * t)) * envelope(t, 0.002, 0.12)
    metal = sum(np.sin(2 * np.pi * f * t) * a for f, a in [(310, 1.0), (467, 0.7), (733, 0.5), (1210, 0.3)])
    metal *= envelope(t, 0.001, 0.18)
    scrape = bandpass(rng.normal(0, 1, len(t)), 400, 4000) * envelope(t, 0.001, 0.09)
    sound = 1.2 * thud + 0.35 * metal + 0.5 * scrape / np.std(scrape)
    write("crash.wav", fade_out(normalize(np.tanh(sound), 0.85)))


def lamp_on():
    """街灯が灯った音。電気が通るジジッという音のあとに、澄んだ音が上へ抜ける。
    ゲーム側で倍率に合わせて音を高くする。"""
    t = timeline(0.8)
    # ジジッ: 60 Hz で揺れる、こもったノイズ
    buzz = bandpass(rng.normal(0, 1, len(t)), 300, 2500)
    buzz *= (0.6 + 0.4 * np.sign(np.sin(2 * np.pi * 60 * t))) * envelope(t, 0.003, 0.05)
    # 上へ抜ける音: 660 Hz から 1320 Hz へ素早く上がる
    freq = 660 * 2 ** np.clip(t / 0.08, 0, 1)
    phase = 2 * np.pi * np.cumsum(freq) / RATE
    ping = np.sin(phase) + 0.3 * np.sin(2 * phase) * np.exp(-t / 0.1)
    ping *= envelope(np.clip(t - 0.03, 0, None), 0.005, 0.22) * (t > 0.03)
    sound = 0.5 * buzz / (np.std(buzz) + 1e-9) * 0.25 + ping
    write("lamp_on.wav", fade_out(normalize(sound, 0.7)))


def cone_hit():
    """パイロンを弾いた音。軽いプラスチックのコンッ。"""
    t = timeline(0.35)
    knock = np.sin(2 * np.pi * 240 * t * (1 - 0.4 * t)) * envelope(t, 0.001, 0.05)
    shell = bandpass(rng.normal(0, 1, len(t)), 900, 3500) * envelope(t, 0.001, 0.03)
    sound = knock + 0.6 * shell / (np.std(shell) + 1e-9) * 0.3
    write("cone_hit.wav", fade_out(normalize(sound, 0.75)))


def drum_hit():
    """ドラム缶を弾いた音。中が空の金属が、ゴワンと長く鳴る。"""
    t = timeline(1.0)
    partials = [(118, 1.0, 0.35), (187, 0.8, 0.3), (296, 0.6, 0.25), (431, 0.45, 0.2), (622, 0.3, 0.12)]
    ring = sum(a * np.sin(2 * np.pi * f * t) * np.exp(-t / d) for f, a, d in partials)
    hit = bandpass(rng.normal(0, 1, len(t)), 300, 3000) * envelope(t, 0.001, 0.025)
    sound = ring + 0.8 * hit / (np.std(hit) + 1e-9) * 0.3
    write("drum_hit.wav", fade_out(normalize(np.tanh(1.3 * sound), 0.8)))


def port_lit():
    """1 周で街灯を全部灯したときのファンファーレ。和音を 2 回重ねて、きらめきで締める。"""
    t = timeline(2.2)
    sound = np.zeros_like(t)
    chords = [(0.0, [523.25, 659.25, 783.99]), (0.18, [698.46, 880.0, 1046.5]), (0.36, [783.99, 987.77, 1174.66, 1567.98])]
    for start, notes in chords:
        local = np.clip(t - start, 0, None)
        gate = t >= start
        decay = 0.9 if start > 0.3 else 0.2
        for freq in notes:
            sound += bell(local, freq) * envelope(local, 0.004, decay) * gate
    sparkle_t = np.clip(t - 0.36, 0, None)
    sparkle = bandpass(rng.normal(0, 1, len(t)), 5000, 12000) * envelope(sparkle_t, 0.02, 0.6) * (t > 0.36)
    sound += sparkle / (np.std(sparkle) + 1e-9) * 0.25
    write("port_lit.wav", fade_out(normalize(sound, 0.85)))


if __name__ == "__main__":
    engine()
    squeal()
    whoosh()
    combo()
    bank()
    crash()
    lamp_on()
    cone_hit()
    drum_hit()
    port_lit()
