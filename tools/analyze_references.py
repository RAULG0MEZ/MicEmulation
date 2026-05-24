#!/usr/bin/env python3
"""Estimate the RODE M2 -> Slate ML-1 corrective EQ from two matched WAV files."""

from __future__ import annotations

import argparse
from pathlib import Path

import numpy as np
from scipy.io import wavfile
from scipy.signal import get_window


def read_wav(path: Path) -> tuple[int, np.ndarray]:
    sample_rate, data = wavfile.read(path)
    if data.ndim > 1:
        data = np.mean(data, axis=1)
    return sample_rate, data.astype(np.float64) / (2**31)


def smooth_log_frequency(freq: np.ndarray, values_db: np.ndarray, sigma_octaves: float = 1.0 / 6.0) -> np.ndarray:
    log_freq = np.log2(freq)
    smoothed = np.empty_like(values_db)
    for index, centre in enumerate(log_freq):
        weights = np.exp(-0.5 * ((log_freq - centre) / sigma_octaves) ** 2)
        smoothed[index] = np.sum(weights * values_db) / np.sum(weights)
    return smoothed


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--source", default="/Users/raulgomez/Desktop/RODE M2.wav")
    parser.add_argument("--target", default="/Users/raulgomez/Desktop/SLATE ML1.wav")
    args = parser.parse_args()

    source_rate, source = read_wav(Path(args.source))
    target_rate, target = read_wav(Path(args.target))

    if source_rate != target_rate:
        raise SystemExit(f"Sample-rate mismatch: {source_rate} vs {target_rate}")

    count = min(len(source), len(target))
    source = source[:count]
    target = target[:count]

    fft_size = 32768
    hop_size = fft_size // 4
    window = get_window("hann", fft_size, fftbins=True)
    frame_count = (count - fft_size) // hop_size + 1

    rms = []
    for frame_index in range(frame_count):
        start = frame_index * hop_size
        mid = 0.5 * (source[start : start + fft_size] + target[start : start + fft_size])
        rms.append(np.sqrt(np.mean(mid * mid) + 1.0e-30))

    rms = np.asarray(rms)
    active_frames = np.where(rms > np.percentile(rms, 35))[0]

    source_spectrum = 0.0
    target_spectrum = 0.0
    for frame_index in active_frames:
        start = frame_index * hop_size
        source_spectrum += np.abs(np.fft.rfft(source[start : start + fft_size] * window)) + 1.0e-12
        target_spectrum += np.abs(np.fft.rfft(target[start : start + fft_size] * window)) + 1.0e-12

    source_spectrum /= len(active_frames)
    target_spectrum /= len(active_frames)

    freq = np.fft.rfftfreq(fft_size, 1.0 / source_rate)
    ratio_db = 20.0 * np.log10(target_spectrum / source_spectrum)
    mask = (freq >= 60.0) & (freq <= 18000.0)
    freq = freq[mask]
    ratio_db = ratio_db[mask]
    smoothed = smooth_log_frequency(freq, ratio_db)

    offset = np.median(smoothed[(freq >= 200.0) & (freq <= 5000.0)])
    tonal_match = smoothed - offset

    print(f"Sample rate: {source_rate} Hz")
    print(f"Duration: {count / source_rate:.2f} sec")
    print(f"Active frames used: {len(active_frames)} / {frame_count}")
    print(f"Broadband offset removed: {offset:.2f} dB")
    print()
    print("Reference points after smoothing and level-normalisation:")
    for point in [60, 80, 100, 150, 200, 250, 315, 400, 500, 630, 800, 1000,
                  1250, 1600, 2000, 2500, 3150, 4000, 5000, 6300, 8000,
                  10000, 12500, 16000, 18000]:
        value = np.interp(np.log2(point), np.log2(freq), tonal_match)
        print(f"{point:6.0f} Hz  {value:7.2f} dB")


if __name__ == "__main__":
    main()
