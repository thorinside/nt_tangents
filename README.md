# Tangents

Steiner-Parker multimode filter plugin for Expert Sleepers disting NT.

## Build

```bash
make
```

Output: `plugins/tangents.o`

Copy to disting NT SD card root folder.

## Controls

| Control | Function |
|---------|----------|
| Left Pot | Input AGR |
| Center Pot | Cutoff (log) |
| Right Pot | Resonance |
| Left Encoder | Mode (LP/BP/HP/AP) |
| Right Encoder | Model (YU/MS/XX) |

## Parameters

### Filter Page

| Parameter | Range | Default | Description |
|-----------|-------|---------|-------------|
| Cutoff | 20-20000 Hz | 1000 Hz | Filter cutoff frequency |
| Resonance | 0-100% | 0% | Filter resonance |
| Mode | LP/BP/HP/AP | LP | Filter mode |
| Model | YU/MS/XX | YU | Saturation model |
| Oversample | 1x-16x | 2x | Oversampling factor |

### Input Page

| Parameter | Range | Default | Description |
|-----------|-------|---------|-------------|
| Input | 0-100% | 50% | AGR: 0-25=random, 25-50=atten, 50=unity, 50-100=amp |
| Drive | 0-100% | 0% | Pre-filter saturation |

### CV Page

| Parameter | Range | Default | Description |
|-----------|-------|---------|-------------|
| CV Cutoff | Bus | - | CV input for cutoff (1V/oct) |
| CV Cut Amt | -100 to +100% | 100% | CV cutoff modulation amount |
| CV Res | Bus | - | CV input for resonance |
| CV Res Amt | -100 to +100% | 100% | CV resonance modulation amount |

### Routing Page

| Parameter | Range | Default | Description |
|-----------|-------|---------|-------------|
| Input | Bus 1-16 | 1 | Audio input bus |
| Output | Bus 1-16 | 1 | Audio output bus |
| Output Mode | Add/Replace | Replace | Output mix mode |

## Models

- **YU** - Smooth tanh saturation (Yusynth-style)
- **MS** - Asymmetric diode clipping
- **XX** - Aggressive fold-back distortion

## License

MIT
