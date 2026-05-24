# MicEmulation

AU/VST3 macOS plugin that applies surgical EQ curves to move source microphone recordings toward the measured tonal profile of a Slate Digital ML-1 reference.

Current source mic profiles:

- RODE M2 -> Slate ML-1
- Shure SM7B -> Slate ML-1

This version is intentionally focused:

- EQ-only processing.
- No saturation, compression, AI, convolution, or advanced physical mic modelling.
- APVTS parameters for automation: `Blend` and `Source Mic`.
- `Blend` ranges from 0% to 200%; 100% is the base measured emulation and 200% exaggerates the correction curve.
- AU for Logic Pro/GarageBand/MainStage and VST3 for DAWs that support VST3.

Pro Tools AAX is not included because it requires the AAX SDK/license and a separate signing/distribution flow.

## Build

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

The first configure downloads JUCE 8.0.13 with CMake FetchContent.

## Build Installer

```sh
./scripts/build_macos_installer.sh
```

Output:

- `dist/MicEmulation-macOS.pkg`

The installer places:

- AU: `/Library/Audio/Plug-Ins/Components/MicEmulation.component`
- VST3: `/Library/Audio/Plug-Ins/VST3/MicEmulation.vst3`

## Re-analyse References

```sh
python3 tools/analyze_references.py
```
