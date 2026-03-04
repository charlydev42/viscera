# Viscera Website

Product website for Viscera, a biomorphic FM synthesizer plugin (VST3/AU/Standalone) for macOS.

## Stack

- Static HTML/CSS/JS — no framework, no build step
- WebGL2 for the flubber visualizer (raymarched blob)
- Vanilla JS only

## Design System

The website mirrors the plugin's **neumorphic** UI aesthetic.

### Color Palette

| Token | Dark Mode | Light Mode |
|-------|-----------|------------|
| Background | `#2E3440` | `#E0E5EC` |
| Surface | `#353C4A` | `#E4E9F0` |
| Elevated | `#3B4252` | `#D8DDE4` |
| Text | `#D8DEE9` | `#404048` |
| Text dim | `#8891A0` | `#808088` |
| Shadow dark | `#1A1E26` | `#A3B1C6` |
| Shadow light | `#434C5E` | `#FFFFFF` |
| **Accent** | `#8BC34A` | `#8BC34A` |

### Typography

- **Font**: System monospace stack — `'SF Mono', 'Fira Code', 'JetBrains Mono', 'Menlo', 'Consolas', monospace`
- No custom web fonts loaded (matches plugin's system monospace approach)

### Neumorphic Shadows

All elevated elements use dual shadows:
```css
box-shadow:
    Npx Npx (N*2)px var(--shadow-dark),
    -Npx -Npx (N*2)px var(--shadow-light);
```
Inset elements invert with `inset`.

### Accent Color

Lime green `#8BC34A` is the **only** vibrant color. Used for:
- Active/interactive states
- Buttons text
- Feature icons
- Toggle LED glow

## Key Files

| File | Purpose |
|------|---------|
| `index.html` | Homepage — hero with flubber, features, download |
| `style.css` | Full neumorphic design system with dark/light themes |
| `flubber.js` | WebGL2 raymarched blob renderer (adapted from plugin GLSL) |
| `assets/` | Logo PNGs for both themes |

## The Flubber

The flubber is a **WebGL2 raymarched metaball blob** — a direct port of the plugin's OpenGL visualizer.

### How it works
- Fragment shader does raymarching against a field of 4-8 ellipsoid blobs blended with smooth minimum
- PBR shading with 4 lights, subsurface scattering, caustics, domain-warped noise coloring
- On the website: **mouse-interactive** instead of audio-reactive
  - Mouse proximity to center = blob "energy"
  - Mouse velocity = blob reactivity
  - Idle state = gentle breathing animation
- Synthetic audio texture (512x2 float) generated from mouse state each frame
- Two shader variants: dark mode and light mode (different env maps, color palettes, tonemapping)

### Performance
- DPR capped at 1.5x to keep frame rate smooth
- 64 raymarching steps (reduced from 96/128 in plugin)
- Pauses rendering when tab is hidden
- Canvas is full-viewport behind hero content

### Shader structure in flubber.js
- `SHARED_GLSL` — noise, SDF, PBR functions, blob geometry (shared between themes)
- `DARK_TAIL` — dark mode environment, colors, shading, mainImage
- `LIGHT_TAIL` — light mode variant
- `window.flubberSetTheme(theme)` — switches active shader program

## Assets

Logo variants in `assets/`:
- `viscera_logo_dark.png` — white logo for dark backgrounds (1213x514)
- `viscera_logo_light.png` — dark logo for light backgrounds (1213x514)
- `viscera_logo_dark_nodolph.png` — compact, no "nodolph" subtitle (1213x191)
- `viscera_logo_light_nodolph.png` — compact light variant (1213x191)
- `viscera_logo_neutral.png` / `viscera_logo_neutral_dark.png` — neutral variants (1920x1080)
- `logo_noir.svg` — vector logo (black)

## Theme Switching

- Theme stored in `localStorage('viscera-theme')`
- `data-theme` attribute on `<html>` controls CSS variables
- Logo images swap on theme change
- Flubber shader program switches via `window.flubberSetTheme()`

## Development

Open `index.html` directly in a browser, or serve locally:
```bash
python3 -m http.server 8000
# or
npx serve .
```

WebGL2 is required. Tested on Chrome, Firefox, Safari 15+.

## Future Enhancements

- Audio input mode: use Web Audio API + getUserMedia to make flubber react to mic/system audio
- Preset browser: interactive demo of factory presets with Web Audio FM synthesis
- Waveform animations on feature cards
- Additional pages: documentation, changelog, about
- Web Audio demo player for preset previews
