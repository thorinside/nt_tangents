# Technical Research Report: Porting Tangents to Disting NT

**Date:** 2025-11-29
**Prepared by:** Neal
**Project Context:** Feasibility study for porting Vult Tangents (Steiner-Parker filter) to Expert Sleepers Disting NT

---

## Executive Summary

**Primary Finding:** Direct porting of Tangents is **NOT VIABLE** due to closed-source licensing. However, creating a Steiner-Parker filter for Disting NT is **HIGHLY VIABLE** through alternative approaches.

### Key Recommendation

**Primary Choice:** Write a custom Steiner-Parker filter implementation using either Vult language (compiled to C++) or directly in C++ for the Disting NT C++ API.

**Rationale:** The Tangents source code is proprietary (closed source since ~2019). However, the underlying Steiner-Parker filter topology is well-documented through public schematics (YuSynth), academic papers, and DSP community discussions. The Vult compiler is open source and can generate C++ suitable for embedded platforms like the Disting NT's ARM Cortex-M7.

**Key Benefits:**
- Full control over the implementation
- No licensing concerns
- Leverage existing Disting NT API knowledge (you're already a contributor!)
- Well-documented filter topology available

---

## 1. Research Objectives

### Technical Question

Can the Tangents Eurorack module (built with the Vult DSP language) be ported to the Expert Sleepers Disting NT platform as a native plugin?

### Project Context

- **Type:** Greenfield port of existing module concept
- **Source Platform:** VCV Rack (Vult Modules)
- **Target Platform:** Disting NT (ARM Cortex-M7, C++ API)
- **Integration Pattern:** Vult → C++ → Disting NT Plugin

---

## 2. Technology Profiles

### 2.1 Vult DSP Language [Verified 2025 Source]

**Overview:**
Vult is a transcompiler that generates high-performance DSP code for embedded systems and audio applications.

**Code Generation:**
- Generates C/C++ with floating-point or fixed-point arithmetic
- Fixed-point uses q16.16 format (suitable for FPU-less MCUs)
- Command: `vultc -ccode -real fixed input.vult -o output`
- Outputs: `.h`, `.cpp`, and `_tables.h` files
- Runtime dependency: `vultin.h` and `vultin.cpp`

**Platform Compatibility:**
- Teensy (including Audio Library templates)
- Arduino/AVR
- Raspberry Pi Pico
- Pure Data externals
- VCV Rack modules
- Browser (JavaScript/WebAudio)

**Current Status:**
- Active development (last GitHub update: 2024-2025)
- Maintained by Leonardo Laguna Ruiz
- MIT licensed (compiler only)

**Sources:**
- [Vult GitHub](https://github.com/vult-dsp/vult)
- [Vult Documentation](https://vult-dsp.github.io/vult/overview/)
- [C++ Generation Tutorial](http://vult-dsp.github.io/vult/tutorials/generate-c/)

---

### 2.2 Tangents Module [Verified 2025 Source]

**Overview:**
Tangents is a virtual analog Steiner-Parker filter implementation for VCV Rack, created by Leonardo Laguna Ruiz (Vult DSP).

**Technical Details:**
- Based on Yves Usson (YuSynth) Steiner-Parker design
- Modeled using electrical components and differential equations
- Three inputs exposed (LP, BP, HP) for multi-source filtering
- Paid version includes 3 implementation variants

**⚠️ CRITICAL FINDING: Source Code Availability**

**Status: CLOSED SOURCE (Proprietary Freeware)**

The Vult Modules source code was removed from the public GitHub repository. From [VCV Rack Library Issue #80](https://github.com/VCVRack/library/issues/80):

> "The author has decided to close the source of some of the new plugins. The reason for closing them is because the new plugins have a very substantial amount of work and research. The nature of the code makes it very hard to enforce license terms and fair use."

**Implication:** You cannot directly port or use Tangents code without explicit permission from Leonardo Laguna Ruiz.

**Sources:**
- [Tangents Module Page](https://modlfo.github.io/VultModules/tangents/)
- [VCV Library - Tangents](https://library.vcvrack.com/VultModulesFree/Tangents)
- [GitHub Issue on Source Availability](https://github.com/VCVRack/library/issues/80)

---

### 2.3 Disting NT C++ API [Verified 2025 Source]

**Overview:**
The Expert Sleepers Disting NT is a multifunction Eurorack module with a C++ plugin API for custom algorithm development.

**Technical Details:**
- **Processor:** ARM Cortex-M7
- **API Version:** v1.10.0 (as of August 2025)
- **Introduced:** Firmware 1.7.0 (March 2025)
- **License:** MIT
- **Contributors:** 2 (including you, Neal!)

**Repository Structure:**
- `include/distingnt/` - Header files
- `examples/` - Example implementations
- `faust/` - Faust DSP integration
- Airwindows audio processing components

**Requirements:**
- ARM GCC toolchain
- Understanding of real-time DSP constraints
- Familiarity with Disting NT's I/O model

**Sources:**
- [Disting NT API GitHub](https://github.com/expertsleepersltd/distingNT_API)
- [Disting NT Main Page](https://www.expert-sleepers.co.uk/distingNT.html)
- [Synthtopia Announcement](https://www.synthtopia.com/content/2025/03/13/expert-sleepers-disting-nt-gets-c-support-so-you-can-code-run-custom-plugins/)

---

### 2.4 Steiner-Parker Filter Topology [High Confidence]

**Overview:**
The Steiner-Parker filter is a balanced/differential form of the Sallen-Key structure, originally published in Electronic Design 25, December 6, 1974.

**Key Characteristics:**
- Multi-mode (LP, BP, HP, AP)
- Self-oscillating in all modes
- 24dB/octave rolloff
- Characteristic "aggressive" resonance

**Circuit Analysis (from KVR DSP Forum):**
- Differential path with two diodes on each side
- Non-linearity approximated by tanh(x): `(e^x - e^-x)/(e^x + e^-x)`
- Requires 4x oversampling for stability at high resonance
- Can be modeled using "Mystran's Cheap method" for non-linearities

**Available Resources:**
- [YuSynth Steiner VCF Schematic](https://yusynth.net/Modular/EN/STEINERVCF/index-v2.html) - Full hardware schematic
- [KVR DSP Forum Discussion](https://www.kvraudio.com/forum/viewtopic.php?t=428719) - Implementation approaches
- [Eddy Bergman Build Guide](https://www.eddybergman.com/2020/04/synthesizer-build-part-26-steiner-vcf.html) - Detailed build notes
- [Vult Analog Steiner-Parker Analysis](https://modlfo.github.io/projects/vult-analog-steiner-parker/) - Leonardo's original analysis

**Mathematical Model (from KVR):**
```
The "real" Steiner-Parker schematics have a differential path
with two diodes on each side. The differential pairs of diodes
add up to a tanh-ish shape.

Non-linearity: tanh(x) = (e^x - e^-x)/(e^x + e^-x)
```

---

## 3. Feasibility Analysis

### 3.1 Direct Port of Tangents

| Criterion | Assessment | Notes |
|-----------|------------|-------|
| Source Code Access | ❌ BLOCKED | Closed source since ~2019 |
| Licensing | ❌ BLOCKED | Proprietary freeware |
| Technical Compatibility | ✅ Possible | Vult → C++ → Disting NT is viable path |
| Author Permission | ❓ Unknown | Would require contacting Leonardo |

**Verdict:** Direct port is NOT viable without explicit permission.

---

### 3.2 Custom Implementation Options

#### Option A: Contact Leonardo Laguna Ruiz

| Factor | Assessment |
|--------|------------|
| Effort | Low (initial contact) |
| Risk | High (may decline) |
| Control | Low (depends on terms) |
| Timeline | Unknown |

**How to pursue:**
- Contact via [vult-dsp.com](https://www.vult-dsp.com) or GitHub
- Propose collaboration or licensing arrangement
- Be prepared for any outcome

---

#### Option B: Write Steiner-Parker in Vult Language

| Factor | Assessment |
|--------|------------|
| Effort | Medium-High (DSP implementation) |
| Risk | Low (well-documented topology) |
| Control | High (your code) |
| Timeline | 2-4 weeks for basic implementation |

**Workflow:**
1. Study YuSynth schematic and KVR discussions
2. Write `.vult` source implementing the filter
3. Compile with `vultc -ccode -real fixed` (if using fixed-point)
4. Wrap generated C++ in Disting NT plugin structure
5. Test and iterate

**Advantages:**
- Vult handles DSP boilerplate
- Can target other platforms later (VCV Rack, etc.)
- Familiar syntax for DSP work

**Resources:**
- [Vult RackPlayground Template](https://github.com/vult-dsp/RackPlayground) - Starting point
- [Vult C++ Generation Tutorial](http://vult-dsp.github.io/vult/tutorials/generate-c/)

---

#### Option C: Write Directly in C++ for Disting NT

| Factor | Assessment |
|--------|------------|
| Effort | Medium (direct implementation) |
| Risk | Low (you know the platform) |
| Control | Highest (no intermediate tools) |
| Timeline | 1-3 weeks for basic implementation |

**Workflow:**
1. Study filter topology and DSP approaches
2. Implement directly in C++ using Disting NT API
3. Handle oversampling (4x recommended)
4. Implement non-linearity (tanh approximation)
5. Test and tune

**Advantages:**
- No Vult dependency
- Direct optimization for Cortex-M7
- Leverage existing Disting NT API knowledge
- Access to existing Disting NT examples

---

## 4. Comparative Analysis

| Criterion | Option A (License) | Option B (Vult) | Option C (C++) |
|-----------|-------------------|-----------------|----------------|
| Time to Start | Immediate | Immediate | Immediate |
| Development Time | Unknown | 2-4 weeks | 1-3 weeks |
| Risk Level | High | Low | Low |
| Code Control | Low | High | Highest |
| Platform Flexibility | Low | High | Medium |
| Learning Required | Low | Medium (Vult) | Low |
| Your Expertise Match | N/A | Medium | High |

---

## 5. Recommendations

### Primary Recommendation: Option C (Direct C++ Implementation)

**Rationale:**
1. You're already a contributor to the Disting NT API - you know the platform
2. Steiner-Parker topology is well-documented
3. No external tool dependencies
4. Fastest path to working plugin
5. Full optimization control for ARM Cortex-M7

### Secondary Recommendation: Option B (Vult Implementation)

**Use this if:**
- You want to target multiple platforms (VCV Rack, other hardware)
- You prefer Vult's DSP-focused syntax
- You want to learn Vult for future projects

### Fallback: Option A (Contact Leonardo)

**Consider this if:**
- You specifically want the exact Tangents sound/behavior
- You're willing to negotiate licensing terms
- You have a commercial interest that justifies the complexity

---

## 6. Disting NT Implementation Details

Based on the Disting NT C++ API v9, here's a concrete implementation plan:

### 6.1 Plugin Structure

```cpp
#include <distingnt/api.h>
#include <new>
#include <cmath>

// Filter state in DTC (fastest memory) for hot path
struct _SteinerParker_DTC {
    float lp_state[2];      // Lowpass filter state
    float bp_state[2];      // Bandpass filter state
    float hp_state[2];      // Highpass filter state
    float feedback;         // Resonance feedback
    float freq_coeff;       // Precomputed frequency coefficient
    float res_coeff;        // Precomputed resonance coefficient
};

struct _SteinerParkerAlgorithm : public _NT_algorithm {
    _SteinerParkerAlgorithm(_SteinerParker_DTC* dtc_) : dtc(dtc_) {}
    _SteinerParker_DTC* dtc;
};
```

### 6.2 Parameter Definitions

```cpp
enum {
    kParamInput,
    kParamOutput,
    kParamOutputMode,
    kParamCutoff,       // 20-20000 Hz
    kParamResonance,    // 0-100%
    kParamMode,         // LP/BP/HP
    kParamCvInput,      // CV modulation input
    kParamCvAmount,     // CV modulation depth
};

static const char* enumStringsMode[] = {
    "Lowpass",
    "Bandpass",
    "Highpass",
    NULL
};

static const _NT_parameter parameters[] = {
    NT_PARAMETER_AUDIO_INPUT("Input", 1, 1)
    NT_PARAMETER_AUDIO_OUTPUT_WITH_MODE("Output", 1, 13)
    { .name = "Cutoff", .min = 20, .max = 20000, .def = 1000, .unit = kNT_unitHz },
    { .name = "Resonance", .min = 0, .max = 100, .def = 0, .unit = kNT_unitPercent },
    { .name = "Mode", .min = 0, .max = 2, .def = 0, .unit = kNT_unitEnum, .enumStrings = enumStringsMode },
    NT_PARAMETER_CV_INPUT("CV", 0, 0)
    { .name = "CV Amt", .min = -100, .max = 100, .def = 50, .unit = kNT_unitPercent, .scaling = kNT_scaling100 },
};
```

### 6.3 GUID Convention

Following the Disting NT GUID convention (Developer ID + Plugin ID):

```cpp
// Neal Sanche = "Ns", Steiner-Parker = "Sp"
.guid = NT_MULTICHAR('N', 's', 'S', 'p')
```

### 6.4 Memory Requirements

```cpp
void calculateRequirements(_NT_algorithmRequirements& req, const int32_t* specs) {
    req.numParameters = ARRAY_SIZE(parameters);
    req.sram = sizeof(_SteinerParkerAlgorithm);
    req.dtc = sizeof(_SteinerParker_DTC);  // Hot filter state in fastest memory
    req.dram = 0;  // No large buffers needed
    req.itc = 0;
}
```

### 6.5 Filter Processing (Simplified)

```cpp
// tanh approximation for non-linearity (fast, ARM-optimized)
inline float fastTanh(float x) {
    float x2 = x * x;
    return x * (27.0f + x2) / (27.0f + 9.0f * x2);
}

void step(_NT_algorithm* self, float* busFrames, int numFramesBy4) {
    _SteinerParkerAlgorithm* pThis = (_SteinerParkerAlgorithm*)self;
    _SteinerParker_DTC* dtc = pThis->dtc;

    int numFrames = numFramesBy4 * 4;

    const float* in = busFrames + (pThis->v[kParamInput] - 1) * numFrames;
    float* out = busFrames + (pThis->v[kParamOutput] - 1) * numFrames;
    bool replace = pThis->v[kParamOutputMode];

    // Get CV if connected
    const float* cv = (pThis->v[kParamCvInput] > 0)
        ? busFrames + (pThis->v[kParamCvInput] - 1) * numFrames
        : nullptr;
    float cvAmt = pThis->v[kParamCvAmount] / 100.0f;

    int mode = pThis->v[kParamMode];

    for (int i = 0; i < numFrames; ++i) {
        // Apply CV modulation to cutoff
        float freq = dtc->freq_coeff;
        if (cv) {
            freq *= expf(cv[i] * cvAmt * 5.0f);  // 1V/oct scaling
        }

        // Steiner-Parker filter core with tanh non-linearity
        float input = in[i];
        float fb = dtc->feedback * dtc->res_coeff;

        // Apply differential non-linearity
        input = fastTanh(input - fb);

        // State variable filter implementation
        // ... (full implementation based on YuSynth topology)

        float output;
        switch (mode) {
            case 0: output = dtc->lp_state[0]; break;  // LP
            case 1: output = dtc->bp_state[0]; break;  // BP
            case 2: output = dtc->hp_state[0]; break;  // HP
            default: output = dtc->lp_state[0];
        }

        out[i] = replace ? output : (out[i] + output);
    }
}
```

### 6.6 Factory Definition

```cpp
static const _NT_factory factory = {
    .guid = NT_MULTICHAR('N', 's', 'S', 'p'),
    .name = "Steiner-Parker",
    .description = "Steiner-Parker multimode filter",
    .numSpecifications = 0,
    .specifications = NULL,
    .calculateStaticRequirements = NULL,
    .initialise = NULL,
    .calculateRequirements = calculateRequirements,
    .construct = construct,
    .parameterChanged = parameterChanged,
    .step = step,
    .draw = NULL,
    .midiRealtime = NULL,
    .midiMessage = NULL,
    .tags = kNT_tagFilterEQ,
    .hasCustomUi = NULL,
    .customUi = NULL,
    .setupUi = NULL,
};
```

### 6.7 Build & Deploy

```bash
# Build for hardware
make hardware

# Deploy to Disting NT
cp plugins/steiner_parker.o /Volumes/DISTINGNT/

# Or test with nt_emu first
make test
# Load in VCV Rack nt_emu module
```

---

## 7. Implementation Roadmap

### Phase 1: Research & Design (3-5 days)
- [ ] Study YuSynth schematic in detail
- [ ] Review KVR DSP forum implementation approaches
- [ ] Design digital model (difference equations)
- [ ] Choose oversampling strategy
- [ ] Design non-linearity approximation

### Phase 2: Core Implementation (1-2 weeks)
- [ ] Implement basic filter structure
- [ ] Add resonance/feedback path
- [ ] Implement LP/BP/HP mode switching
- [ ] Add tanh non-linearity
- [ ] Implement oversampling

### Phase 3: Disting NT Integration (3-5 days)
- [ ] Create plugin structure using API
- [ ] Map CV inputs to filter parameters
- [ ] Implement parameter smoothing
- [ ] Handle audio I/O

### Phase 4: Testing & Tuning (1 week)
- [ ] Compare to reference recordings
- [ ] Tune frequency response
- [ ] Optimize CPU usage
- [ ] Test edge cases (high resonance, etc.)

---

## 7. Risk Mitigation

| Risk | Likelihood | Impact | Mitigation |
|------|------------|--------|------------|
| DSP instability at high resonance | Medium | High | 4x oversampling, careful coefficient design |
| CPU overload on Cortex-M7 | Low | Medium | Profile early, use fixed-point if needed |
| Sound doesn't match expectations | Medium | Medium | Reference recordings, iterative tuning |
| Vult compilation issues | Low | Low | Fallback to direct C++ |

---

## 8. References and Sources

### Official Documentation
- [Vult DSP GitHub](https://github.com/vult-dsp/vult)
- [Vult Language Documentation](https://vult-dsp.github.io/vult/overview/)
- [Disting NT API GitHub](https://github.com/expertsleepersltd/distingNT_API)
- [Expert Sleepers Disting NT](https://www.expert-sleepers.co.uk/distingNT.html)

### Filter Design Resources
- [YuSynth Steiner-Parker VCF](https://yusynth.net/Modular/EN/STEINERVCF/index-v2.html)
- [KVR DSP Forum - Steiner Parker Topology](https://www.kvraudio.com/forum/viewtopic.php?t=428719)
- [Vult Analog Steiner-Parker Analysis](https://modlfo.github.io/projects/vult-analog-steiner-parker/)
- [Eddy Bergman Build Guide](https://www.eddybergman.com/2020/04/synthesizer-build-part-26-steiner-vcf.html)

### Community Resources
- [Tangents Module Page](https://modlfo.github.io/VultModules/tangents/)
- [VCV Library - Tangents](https://library.vcvrack.com/VultModulesFree/Tangents)
- [ModWiggler Disting NT Thread](https://www.modwiggler.com/forum/viewtopic.php?t=287910)

---

## Document Information

**Workflow:** BMad Research Workflow - Technical Research
**Generated:** 2025-11-29
**Research Type:** Technical Feasibility Study
**Confidence Level:** High (verified 2025 sources)

---

*This technical research report was generated using the BMad Method Research Workflow. All version numbers and technical claims are backed by current 2025 sources where noted.*
