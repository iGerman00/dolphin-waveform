# Dolphin Waveform

A KF6 KIO thumbnail creator that renders embedded album artwork with a waveform underneath it. Dolphin's Information panel continues to provide the native media controls and metadata.

![Dolphin Waveform preview](screenshot.png)

## Features

- Preserves embedded album artwork and renders a waveform beneath it.
- Uses Dolphin's native playback controls, seeking, and metadata panel.
- Supports configurable layout, artwork fitting, colors, KDE color-scheme tokens, and waveform analysis.
- Reads configuration per user without requiring a rebuild.
- Declines small preview requests by default to avoid most grid and inline thumbnails.

## Configuration

Copy `dolphin-waveform.conf.example` to `~/.config/dolphin-waveform.conf`. The thumbnailer reads this per-user file for every preview request, so changes take effect without rebuilding. KIO thumbnail caching is disabled so configuration changes are not hidden by old images.

The `[Layout]` group controls padding, artwork fitting, waveform height, separator, slider alignment, minimum preview width, and whether the artwork background is square-only. The default `minimumPreviewWidth=129` prevents the waveform thumbnailer from running for ordinary small grid thumbnails; set it to `0` to enable all sizes. Set `artBackgroundSquareOnly=false` for a full-width artwork frame, especially with `imageMode=cover`. `[Colors]` controls the canvas background, missing-artwork background, waveform, separator, and the `accent` alias. Colors may be hex values, `transparent`, `accent`, or canonical KDE scheme tokens. If `accent` is omitted, it resolves to KDE's current selection accent, so light/dark theme changes are reflected automatically. `[Waveform]` controls bin count, analysis window, exponent, minimum bar height, and spacing. Values are clamped to safe ranges.

## Build

Dependencies:

- Qt 6 (Core and Gui)
- KDE Frameworks 6 (KCoreAddons and KIO)
- FFmpeg development packages

```sh
cmake -B build
cmake --build build
sudo cmake --install build
```

The plugin installs system-wide to `/usr/lib/qt6/plugins/kf6/thumbcreator/`, which is where system Qt/KIO searches for it. Restart Dolphin after installation. Enable the generated `Audio Waveform` thumbnailer in Dolphin's preview settings if another audio thumbnailer takes precedence.

The waveform is a static image regenerated for each preview request. The native Dolphin seek slider remains the interactive control.

## Tested Environment

- Dolphin 26.08.0
- EndeavourOS
- KDE Plasma 6.7.4
- KDE Frameworks 6.29.0
- Qt 6.11.2
- Linux kernel 7.2.3-arch1-2 (64-bit)
- Wayland
