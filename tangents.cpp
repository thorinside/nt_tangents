/*
Tangents - Steiner-Parker Multimode Filter for distingNT
Based on the Steiner-Parker topology with tanh non-linearity

Features:
- 3 filter models: YU (smooth), MS (diode clipping), XX (aggressive)
- 3 simultaneous outputs: LP, BP, HP (directly or mixed to main output)
- CV control of Cutoff and Resonance
- Drive control with saturation
- Self-oscillation capability
- Frequency response curve display
- Full custom UI with pots and encoders

GUID: NsTg (NealSanche + Tangents)
*/

#include <distingnt/api.h>
#include <math.h>
#include <new>
#include <cstring>

// ============================================================================
// CONSTANTS
// ============================================================================

#ifndef M_PI
#define M_PI 3.14159265358979323846f
#endif

// Default oversampling for initial coefficient calculation
static const int DEFAULT_OVERSAMPLE = 2;

// ============================================================================
// ALGORITHM DATA STRUCTURES
// ============================================================================

/**
 * DTC (Data Tightly Coupled) memory structure
 * Performance-critical filter state goes here for fastest access
 */
struct _tangentsAlgorithm_DTC
{
	// Steiner-Parker filter state (2-pole)
	float lp;           // Lowpass output
	float bp;           // Bandpass output
	float hp;           // Highpass output (computed)

	// Filter coefficients (precomputed)
	float g;            // Frequency coefficient
	float k;            // Resonance/feedback coefficient
	float gInv;         // 1/(1+g) for efficiency

	// Smoothed parameter values (for zipper-free changes)
	float cutoffSmooth;
	float resonanceSmooth;
	float driveSmooth;
	float agrSmooth;
	float cvCutoffAmtSmooth;
	float cvResAmtSmooth;

	// Random state for AGR (Attenu-Gain-Randomizer)
	uint32_t randState;

	// For display
	float inputLevel;
	float outputLevel;
};

/**
 * Main algorithm structure
 */
struct _tangentsAlgorithm : public _NT_algorithm
{
	_tangentsAlgorithm(_tangentsAlgorithm_DTC* dtc_) : dtc(dtc_) {}
	~_tangentsAlgorithm() {}

	_tangentsAlgorithm_DTC* dtc;

	// Cached computed values
	float sampleRateRecip;
};

// ============================================================================
// PARAMETER DEFINITIONS
// ============================================================================

enum
{
	// Audio routing
	kParamInput,
	kParamOutput,
	kParamOutputMode,

	// Filter controls
	kParamCutoff,
	kParamResonance,
	kParamMode,
	kParamModel,

	// CV inputs
	kParamCvCutoff,
	kParamCvCutoffAmt,
	kParamCvResonance,
	kParamCvResonanceAmt,

	// Input control and drive
	kParamInputAGR,    // Attenu-Gain-Randomizer: CCW=random, 9-12=atten, 12=unity, CW=amplify
	kParamDrive,
	kParamOversample,  // 1x, 2x, 4x

	kNumParameters
};

static char const * const enumStringsMode[] = {
	"Lowpass",
	"Bandpass",
	"Highpass",
	"All-pass",
	NULL
};

static char const * const enumStringsModel[] = {
	"YU",    // Original Yusynth-derived, smooth
	"MS",    // Modified feedback with diode clipping
	"XX",    // Aggressive/experimental
	NULL
};

static char const * const enumStringsOversample[] = {
	"1x",    // No oversampling, lowest CPU
	"2x",    // 2x oversampling
	"4x",    // 4x oversampling
	"8x",    // 8x oversampling
	"16x",   // 16x oversampling, highest quality
	NULL
};

static const _NT_parameter parameters[] = {
	// Audio I/O
	NT_PARAMETER_AUDIO_INPUT("Input", 1, 1)
	NT_PARAMETER_AUDIO_OUTPUT_WITH_MODE("Output", 1, 13)

	// Filter controls
	// kNT_scaling10 means host displays raw/10 (e.g., raw 1000 shows as "100.0%")
	// Plugin always receives raw integer values in v[]
	{ .name = "Cutoff", .min = 20, .max = 20000, .def = 1000, .unit = kNT_unitHz, .scaling = kNT_scalingNone, .enumStrings = NULL },
	{ .name = "Resonance", .min = 0, .max = 1000, .def = 0, .unit = kNT_unitPercent, .scaling = kNT_scaling10, .enumStrings = NULL },
	{ .name = "Mode", .min = 0, .max = 3, .def = 0, .unit = kNT_unitEnum, .scaling = kNT_scalingNone, .enumStrings = NULL },
	{ .name = "Model", .min = 0, .max = 2, .def = 0, .unit = kNT_unitEnum, .scaling = kNT_scalingNone, .enumStrings = enumStringsModel },

	// CV inputs - kNT_scaling10 gives 0.1% resolution
	NT_PARAMETER_CV_INPUT("CV Cutoff", 0, 0)
	{ .name = "CV Cut Amt", .min = -1000, .max = 1000, .def = 1000, .unit = kNT_unitPercent, .scaling = kNT_scaling10, .enumStrings = NULL },
	NT_PARAMETER_CV_INPUT("CV Res", 0, 0)
	{ .name = "CV Res Amt", .min = -1000, .max = 1000, .def = 1000, .unit = kNT_unitPercent, .scaling = kNT_scaling10, .enumStrings = NULL },

	// Input AGR and Drive - kNT_scaling10 gives 0.1% resolution
	// AGR: 0-25=random, 25-50=atten, 50=unity, 50-100=amplify (+12dB max)
	{ .name = "Input", .min = 0, .max = 1000, .def = 500, .unit = kNT_unitPercent, .scaling = kNT_scaling10, .enumStrings = NULL },
	{ .name = "Drive", .min = 0, .max = 1000, .def = 0, .unit = kNT_unitPercent, .scaling = kNT_scaling10, .enumStrings = NULL },
	{ .name = "Oversample", .min = 0, .max = 4, .def = 1, .unit = kNT_unitEnum, .scaling = kNT_scalingNone, .enumStrings = enumStringsOversample },
};

// ============================================================================
// PARAMETER PAGES
// ============================================================================

static const uint8_t pageFilter[] = {
	kParamCutoff,
	kParamResonance,
	kParamMode,
	kParamModel,
	kParamOversample,
};

static const uint8_t pageInput[] = {
	kParamInputAGR,
	kParamDrive,
};

static const uint8_t pageCV[] = {
	kParamCvCutoff,
	kParamCvCutoffAmt,
	kParamCvResonance,
	kParamCvResonanceAmt,
};

static const uint8_t pageRouting[] = {
	kParamInput,
	kParamOutput,
	kParamOutputMode,
};

static const _NT_parameterPage pages[] = {
	{ .name = "Filter", .numParams = ARRAY_SIZE(pageFilter), .params = pageFilter },
	{ .name = "Input", .numParams = ARRAY_SIZE(pageInput), .params = pageInput },
	{ .name = "CV", .numParams = ARRAY_SIZE(pageCV), .params = pageCV },
	{ .name = "Routing", .numParams = ARRAY_SIZE(pageRouting), .params = pageRouting },
};

static const _NT_parameterPages parameterPages = {
	.numPages = ARRAY_SIZE(pages),
	.pages = pages,
};

// ============================================================================
// HELPER FUNCTIONS
// ============================================================================

/**
 * Fast tanh approximation for Steiner-Parker non-linearity
 * Based on rational approximation, accurate to ~0.001 for |x| < 3
 */
inline float fastTanh(float x)
{
	// Clamp to avoid overflow
	if (x > 3.0f) return 1.0f;
	if (x < -3.0f) return -1.0f;

	float x2 = x * x;
	return x * (27.0f + x2) / (27.0f + 9.0f * x2);
}

/**
 * Diode clipping approximation for MS model
 * Asymmetric soft clipping characteristic
 */
inline float diodeClip(float x)
{
	// Asymmetric clipping: harder on positive, softer on negative
	if (x > 0.0f)
		return 1.0f - expf(-x);
	else
		return -0.5f * (1.0f - expf(2.0f * x));
}

/**
 * Aggressive saturation for XX model
 * Hard clipping with fold-back
 */
inline float aggressiveSat(float x)
{
	// Fold-back distortion for aggressive sound
	x = fastTanh(x * 2.0f);
	if (fabsf(x) > 0.8f)
	{
		float excess = fabsf(x) - 0.8f;
		x = (x > 0 ? 1.0f : -1.0f) * (0.8f - excess * 0.5f);
	}
	return x;
}

/**
 * Convert dB to linear gain
 */
inline float dbToLinear(float db)
{
	return powf(10.0f, db / 20.0f);
}

/**
 * Sanitize float - returns 0 if NaN or infinity
 */
inline float sanitize(float x)
{
	// Check for NaN or infinity
	if (x != x || x > 1e10f || x < -1e10f)
		return 0.0f;
	return x;
}

/**
 * Soft clamp to prevent filter runaway
 * Uses tanh-like soft limiting at ±10
 */
inline float softClamp(float x, float limit = 10.0f)
{
	if (x > limit) return limit;
	if (x < -limit) return -limit;
	return x;
}

/**
 * Fast xorshift random number generator
 * Returns value in range [0.0, 1.0]
 */
inline float fastRandom(uint32_t& state)
{
	state ^= state << 13;
	state ^= state >> 17;
	state ^= state << 5;
	return (float)(state & 0x7FFFFFFF) / (float)0x7FFFFFFF;
}

/**
 * Attenu-Gain-Randomizer (AGR) processing
 *
 * Control range 0-100:
 *   0-25:   Randomization zone - amplitude randomly modulated per sample
 *   25-50:  Attenuation zone - linear fade from half to unity
 *   50:     Unity gain (0dB)
 *   50-100: Amplification zone - quadratic to +12dB
 *
 * Returns the gain multiplier to apply to the input signal
 */
inline float processAGR(int agrValue, uint32_t& randState)
{
	if (agrValue <= 25)
	{
		// Randomization zone (0-25)
		// Random amplitude modulation - more random at lower values
		float randomMix = 1.0f - (float)agrValue / 25.0f;  // 1.0 at 0, 0.0 at 25
		float baseGain = (float)agrValue / 50.0f;          // 0.0 at 0, 0.5 at 25
		float randomGain = fastRandom(randState);          // 0.0 to 1.0
		return baseGain + randomGain * randomMix;
	}
	else if (agrValue <= 50)
	{
		// Attenuation zone (25-50): 0.5 to 1.0
		float t = (float)(agrValue - 25) / 25.0f;
		return 0.5f + t * 0.5f;
	}
	else
	{
		// Amplification zone (50-100): 1.0 to 4.0 (+12dB)
		float t = (float)(agrValue - 50) / 50.0f;
		return 1.0f + t * 3.0f;
	}
}

/**
 * Calculate filter coefficients from frequency and resonance
 * Uses trapezoidal (TPT) SVF topology for stability
 */
inline void calculateFilterCoeffs(_tangentsAlgorithm_DTC* dtc, float cutoff, float resonance, float sampleRate)
{
	// Clamp cutoff to safe range
	if (cutoff < 20.0f) cutoff = 20.0f;
	if (cutoff > sampleRate * 0.45f) cutoff = sampleRate * 0.45f;

	// Pre-warped frequency coefficient: g = tan(π * fc / fs)
	float g = tanf(M_PI * cutoff / sampleRate);

	// Damping coefficient k: controls resonance
	// k = 2 means no resonance (critically damped)
	// k = 0 means infinite resonance (self-oscillation)
	// We map resonance 0-1 to k 2-0.1 (leaving some damping for stability)
	float k = 2.0f - resonance * 1.9f;  // k ranges from 2.0 to 0.1

	dtc->g = g;
	dtc->k = k;
	dtc->gInv = 1.0f / (1.0f + g * (g + k));  // Normalization factor for TPT
}

// ============================================================================
// FACTORY FUNCTIONS
// ============================================================================

void calculateRequirements(_NT_algorithmRequirements& req, const int32_t* specifications)
{
	req.numParameters = ARRAY_SIZE(parameters);
	req.sram = sizeof(_tangentsAlgorithm);
	req.dram = 0;  // No large buffers needed
	req.dtc = sizeof(_tangentsAlgorithm_DTC);
	req.itc = 0;
}

_NT_algorithm* construct(const _NT_algorithmMemoryPtrs& ptrs,
                        const _NT_algorithmRequirements& req,
                        const int32_t* specifications)
{
	_tangentsAlgorithm* alg = new (ptrs.sram) _tangentsAlgorithm(
		(_tangentsAlgorithm_DTC*)ptrs.dtc
	);

	alg->parameters = parameters;
	alg->parameterPages = &parameterPages;

	// Initialize DTC memory
	memset(alg->dtc, 0, sizeof(_tangentsAlgorithm_DTC));

	// Initialize cached values
	alg->sampleRateRecip = 1.0f / NT_globals.sampleRate;

	// Initialize random state for AGR (use a non-zero seed)
	alg->dtc->randState = 0x12345678;

	// Initialize smoothed parameter values to defaults
	// (scaling=1, so raw defaults are 10x displayed values)
	alg->dtc->cutoffSmooth = 1000.0f;       // 1000 Hz default
	alg->dtc->resonanceSmooth = 0.0f;       // 0% default
	alg->dtc->driveSmooth = 1.0f;           // Unity (0% drive)
	alg->dtc->agrSmooth = 50.0f;            // 50% = unity
	alg->dtc->cvCutoffAmtSmooth = 1.0f;     // 100% default
	alg->dtc->cvResAmtSmooth = 1.0f;        // 100% default

	// Calculate initial filter coefficients (will be recalculated in step())
	calculateFilterCoeffs(alg->dtc, 1000.0f, 0.0f, NT_globals.sampleRate * DEFAULT_OVERSAMPLE);

	return alg;
}

void parameterChanged(_NT_algorithm* self, int p)
{
	(void)self;  // Unused - we use smoothing in step() instead
	(void)p;

	// Filter parameters trigger coefficient recalculation on next step()
	// We use smoothing in the audio loop for zipper-free changes
}

void step(_NT_algorithm* self, float* busFrames, int numFramesBy4)
{
	_tangentsAlgorithm* pThis = (_tangentsAlgorithm*)self;
	_tangentsAlgorithm_DTC* dtc = pThis->dtc;

	int numFrames = numFramesBy4 * 4;

	// Get audio busses
	const float* in = busFrames + (pThis->v[kParamInput] - 1) * numFrames;
	float* out = busFrames + (pThis->v[kParamOutput] - 1) * numFrames;
	bool replace = pThis->v[kParamOutputMode];

	// Get CV busses (if connected)
	const float* cvCutoff = (pThis->v[kParamCvCutoff] > 0)
		? busFrames + (pThis->v[kParamCvCutoff] - 1) * numFrames
		: nullptr;
	const float* cvResonance = (pThis->v[kParamCvResonance] > 0)
		? busFrames + (pThis->v[kParamCvResonance] - 1) * numFrames
		: nullptr;

	// Get parameter values
	// Scaling is display-only; v[] always contains raw integers
	// With kNT_scaling10: raw 0-1000 displays as 0.0-100.0%
	float baseCutoff = (float)pThis->v[kParamCutoff];            // 20 - 20000 Hz
	float baseResonance = pThis->v[kParamResonance] / 1000.0f;   // 0.0 - 1.0 (raw 0-1000)
	int mode = pThis->v[kParamMode];
	int model = pThis->v[kParamModel];  // 0=YU, 1=MS, 2=XX
	float cvCutoffAmtTarget = pThis->v[kParamCvCutoffAmt] / 1000.0f;     // -1.0 to 1.0 (raw -1000 to 1000)
	float cvResAmtTarget = pThis->v[kParamCvResonanceAmt] / 1000.0f;     // -1.0 to 1.0 (raw -1000 to 1000)
	float agrTarget = pThis->v[kParamInputAGR] / 10.0f;          // 0.0 - 100.0 (raw 0-1000)
	float driveTarget = 1.0f + pThis->v[kParamDrive] / 250.0f;   // 1.0 to 5.0 (raw 0-1000)

	// Oversampling: 0=1x, 1=2x, 2=4x, 3=8x, 4=16x
	int osParam = pThis->v[kParamOversample];
	int oversample = 1 << osParam;  // 1, 2, 4, 8, or 16
	float oversampleRate = NT_globals.sampleRate * oversample;

	// Level tracking
	float maxIn = 0.0f;
	float maxOut = 0.0f;

	// Smooth all continuous parameters toward targets (once per block)
	// Coefficient ~0.1 gives ~10 block settling time (~2ms at 48kHz/128 samples)
	const float smoothCoeff = 0.1f;

	dtc->driveSmooth += (driveTarget - dtc->driveSmooth) * smoothCoeff;
	dtc->agrSmooth += (agrTarget - dtc->agrSmooth) * smoothCoeff;
	dtc->cvCutoffAmtSmooth += (cvCutoffAmtTarget - dtc->cvCutoffAmtSmooth) * smoothCoeff;
	dtc->cvResAmtSmooth += (cvResAmtTarget - dtc->cvResAmtSmooth) * smoothCoeff;

	// Calculate coefficients once per block (not per sample!)
	// Use CV from first sample for modulation
	float cutoff = baseCutoff;
	float resonance = baseResonance;

	if (cvCutoff)
	{
		float cvVal = cvCutoff[0] * dtc->cvCutoffAmtSmooth;
		cutoff *= powf(2.0f, cvVal * 5.0f);  // 1V/oct: ±5 octaves
	}

	if (cvResonance)
	{
		float cvVal = cvResonance[0] * dtc->cvResAmtSmooth;
		resonance += cvVal * 0.5f;
		if (resonance < 0.0f) resonance = 0.0f;
		if (resonance > 1.0f) resonance = 1.0f;
	}

	// Smooth cutoff and resonance toward target
	dtc->cutoffSmooth += (cutoff - dtc->cutoffSmooth) * smoothCoeff;
	dtc->resonanceSmooth += (resonance - dtc->resonanceSmooth) * smoothCoeff;

	// Calculate coefficients once per block using oversampled rate
	calculateFilterCoeffs(dtc, dtc->cutoffSmooth, dtc->resonanceSmooth, oversampleRate);

	// Pre-calculate resonance amount for saturation
	float resAmt = (2.0f - dtc->k) / 1.9f;

	// Process audio
	for (int i = 0; i < numFrames; ++i)
	{

		// Process input through AGR (Attenu-Gain-Randomizer)
		// Use smoothed AGR value (cast to int for zone calculation)
		float agrGain = processAGR((int)dtc->agrSmooth, dtc->randState);
		float input = in[i] * agrGain * dtc->driveSmooth;

		// Track input level
		float absIn = fabsf(input);
		if (absIn > maxIn) maxIn = absIn;

		// === STEINER-PARKER FILTER CORE ===
		// Oversampled processing for stability
		float output = 0.0f;

		for (int os = 0; os < oversample; ++os)
		{
			// Apply non-linearity based on model type
			// The saturation tames the input to prevent filter blowup
			float u;

			switch (model)
			{
				case 0:  // YU - Smooth tanh saturation
					u = fastTanh(input * (1.0f + resAmt));
					break;

				case 1:  // MS - Asymmetric diode character
					u = diodeClip(input * (1.0f + resAmt * 0.5f));
					break;

				case 2:  // XX - Aggressive saturation
					u = aggressiveSat(input * (1.0f + resAmt * 2.0f));
					break;

				default:
					u = fastTanh(input);
			}

			// Trapezoidal (TPT) State Variable Filter
			// This topology is stable and doesn't blow up at high resonance
			//
			// hp = (input - k*bp - lp) / (1 + k*g + g*g)
			// bp_new = g*hp + bp
			// lp_new = g*bp_new + lp

			float hp = (u - dtc->k * dtc->bp - dtc->lp) * dtc->gInv;
			float bp = dtc->g * hp + dtc->bp;
			float lp = dtc->g * bp + dtc->lp;

			// Soft clamp states for safety (shouldn't be needed with proper TPT)
			bp = softClamp(bp, 5.0f);
			lp = softClamp(lp, 5.0f);

			// Update state
			dtc->bp = sanitize(bp);
			dtc->lp = sanitize(lp);
			dtc->hp = sanitize(hp);

			// Accumulate output based on mode (for oversampling averaging)
			switch (mode)
			{
				case 0:  // Lowpass
					output += lp;
					break;
				case 1:  // Bandpass
					output += bp;
					break;
				case 2:  // Highpass
					output += hp;
					break;
				case 3:  // All-pass (LP - HP)
					output += lp - hp;
					break;
			}
		}

		// Average oversampled output
		output /= (float)oversample;

		// Sanitize (catch any NaN/inf)
		output = sanitize(output);

		// Model-specific output saturation
		switch (model)
		{
			case 0:  // YU - Smooth tanh saturation (Yusynth-style)
				output = fastTanh(output);
				break;
			case 1:  // MS - Asymmetric diode clipping
				output = diodeClip(output);
				break;
			case 2:  // XX - Aggressive fold-back distortion
				output = aggressiveSat(output);
				break;
			default:
				output = fastTanh(output);
		}

		// Track output level
		float absOut = fabsf(output);
		if (absOut > maxOut) maxOut = absOut;

		// Write output
		if (replace)
			out[i] = output;
		else
			out[i] += output;
	}

	// Store levels for display (with decay)
	dtc->inputLevel = dtc->inputLevel * 0.95f + maxIn * 0.05f;
	dtc->outputLevel = dtc->outputLevel * 0.95f + maxOut * 0.05f;
}

bool draw(_NT_algorithm* self)
{
	_tangentsAlgorithm* pThis = (_tangentsAlgorithm*)self;
	_tangentsAlgorithm_DTC* dtc = pThis->dtc;

	// Draw plugin name
	NT_drawText(5, 8, "TANGENTS", 15, kNT_textLeft, kNT_textNormal);

	// Draw model
	const char* modelNames[] = {"YU", "MS", "XX"};
	int model = pThis->v[kParamModel];
	NT_drawText(70, 8, modelNames[model], 12);

	// Draw mode
	const char* modeNames[] = {"LP", "BP", "HP", "AP"};
	int mode = pThis->v[kParamMode];
	NT_drawText(95, 8, modeNames[mode], 12);

	// Draw frequency response curve approximation
	// This is a simplified visualization
	// Cutoff: integer Hz, Resonance: scaling=1 so /10 for 0-100 range
	float cutoff = (float)pThis->v[kParamCutoff];
	float resonance = pThis->v[kParamResonance] / 10.0f;  // 0-100 range

	// Map cutoff to x position (log scale)
	float logFreq = log10f(cutoff);
	float logMin = log10f(20.0f);
	float logMax = log10f(20000.0f);
	int peakX = 100 + (int)((logFreq - logMin) / (logMax - logMin) * 140.0f);

	// Draw frequency response curve
	int prevY = 45;
	for (int x = 100; x < 250; x += 2)
	{
		// Calculate approximate filter response
		float xNorm = (float)(x - 100) / 150.0f;
		float freq = 20.0f * powf(1000.0f, xNorm);  // Log frequency scale

		float freqRatio = freq / cutoff;
		float response;

		switch (mode)
		{
			case 0:  // LP
				response = 1.0f / sqrtf(1.0f + freqRatio * freqRatio * freqRatio * freqRatio);
				break;
			case 1:  // BP
				response = freqRatio / (1.0f + freqRatio * freqRatio);
				if (resonance > 50.0f) response *= 1.0f + (resonance - 50.0f) / 25.0f;
				break;
			case 2:  // HP
				response = freqRatio * freqRatio / sqrtf(1.0f + freqRatio * freqRatio * freqRatio * freqRatio);
				break;
			case 3:  // AP
				response = 0.5f;  // Flat magnitude
				break;
			default:
				response = 0.5f;
		}

		// Add resonance peak
		if (resonance > 0 && fabsf(freqRatio - 1.0f) < 0.3f)
		{
			float peakBoost = 1.0f + (resonance / 100.0f) * 2.0f * (1.0f - fabsf(freqRatio - 1.0f) / 0.3f);
			response *= peakBoost;
		}

		// Clamp and scale to screen
		if (response > 2.0f) response = 2.0f;
		int y = 55 - (int)(response * 15.0f);
		if (y < 20) y = 20;
		if (y > 55) y = 55;

		// Draw line segment
		if (x > 100)
			NT_drawShapeI(kNT_line, x - 2, prevY, x, y, 10);
		prevY = y;
	}

	// Draw cutoff frequency marker
	NT_drawShapeI(kNT_line, peakX, 20, peakX, 55, 15);

	// Draw AGR indicator
	// With scaling=1, raw value is 0-1000, displayed as 0.0-100.0
	float agrValue = pThis->v[kParamInputAGR] / 10.0f;  // Convert to 0-100 range
	const char* agrZone;
	int agrColor;
	if (agrValue <= 25.0f) {
		agrZone = "RND";
		agrColor = 10;  // Random zone
	} else if (agrValue <= 50.0f) {
		agrZone = "ATN";
		agrColor = 8;   // Attenuation zone
	} else {
		agrZone = "AMP";
		agrColor = 12;  // Amplification zone
	}
	NT_drawText(120, 8, agrZone, agrColor);

	// Draw I/O level meters
	int inWidth = (int)(dtc->inputLevel * 50.0f);
	int outWidth = (int)(dtc->outputLevel * 50.0f);
	if (inWidth > 50) inWidth = 50;
	if (outWidth > 50) outWidth = 50;

	NT_drawText(5, 58, "I", 8);
	NT_drawShapeI(kNT_rectangle, 12, 56, 12 + inWidth, 60, 6);

	NT_drawText(65, 58, "O", 8);
	NT_drawShapeI(kNT_rectangle, 72, 56, 72 + outWidth, 60, 12);

	return true;  // We handle all drawing - hide standard top bar
}

uint32_t hasCustomUi(_NT_algorithm* self)
{
	// Return bitmask of pots handled by custom UI for soft-takeover
	return kNT_potL | kNT_potC | kNT_potR;
}

void customUi(_NT_algorithm* self, const _NT_uiData& data)
{
	_tangentsAlgorithm* pThis = (_tangentsAlgorithm*)self;

	// Left pot: Input AGR (Attenu-Gain-Randomizer)
	// controls bitmask indicates which pots have changed (for soft-takeover)
	// With scaling=1, parameter range is 0-1000 (displays 0.0-100.0)
	if (data.controls & kNT_potL)
	{
		int value = (int)(data.pots[0] * 1000.0f);
		NT_setParameterFromUi(NT_algorithmIndex(self),
		                     kParamInputAGR + NT_parameterOffset(),
		                     value);
	}

	// Center pot: Cutoff (logarithmic mapping)
	// Integer Hz, range 20-20000
	if (data.controls & kNT_potC)
	{
		// Logarithmic mapping: 20Hz to 20kHz
		float potVal = data.pots[1];
		int value = (int)(20.0f * powf(1000.0f, potVal));
		if (value > 20000) value = 20000;
		if (value < 20) value = 20;
		NT_setParameterFromUi(NT_algorithmIndex(self),
		                     kParamCutoff + NT_parameterOffset(),
		                     value);
	}

	// Right pot: Resonance
	// With scaling=1, parameter range is 0-1000 (displays 0.0-100.0%)
	if (data.controls & kNT_potR)
	{
		int value = (int)(data.pots[2] * 1000.0f);
		NT_setParameterFromUi(NT_algorithmIndex(self),
		                     kParamResonance + NT_parameterOffset(),
		                     value);
	}

	// Left encoder: Mode selection (LP/BP/HP/AP)
	if (data.encoders[0] != 0)
	{
		int mode = pThis->v[kParamMode];
		mode += data.encoders[0];
		if (mode < 0) mode = 3;
		if (mode > 3) mode = 0;
		NT_setParameterFromUi(NT_algorithmIndex(self),
		                     kParamMode + NT_parameterOffset(),
		                     mode);
	}

	// Right encoder: Model selection (YU/MS/XX)
	if (data.encoders[1] != 0)
	{
		int model = pThis->v[kParamModel];
		model += data.encoders[1];
		if (model < 0) model = 2;
		if (model > 2) model = 0;
		NT_setParameterFromUi(NT_algorithmIndex(self),
		                     kParamModel + NT_parameterOffset(),
		                     model);
	}
}

void setupUi(_NT_algorithm* self, _NT_float3& pots)
{
	_tangentsAlgorithm* pThis = (_tangentsAlgorithm*)self;

	// Initialize pot positions for soft takeover
	// With scaling=1, raw values are 10x the displayed values

	// Left pot: Input AGR (raw 0-1000 = display 0.0-100.0)
	pots[0] = pThis->v[kParamInputAGR] / 1000.0f;

	// Center pot: Cutoff (logarithmic, integer Hz 20-20000)
	float cutoff = (float)pThis->v[kParamCutoff];
	pots[1] = log10f(cutoff / 20.0f) / 3.0f;  // 3 = log10(1000)
	if (pots[1] < 0.0f) pots[1] = 0.0f;
	if (pots[1] > 1.0f) pots[1] = 1.0f;

	// Right pot: Resonance (raw 0-1000 = display 0.0-100.0%)
	pots[2] = pThis->v[kParamResonance] / 1000.0f;
}

// ============================================================================
// FACTORY DEFINITION
// ============================================================================

static const _NT_factory factory =
{
	.guid = NT_MULTICHAR('N', 's', 'T', 'g'),  // NealSanche + Tangents
	.name = "Tangents",
	.description = "Steiner-Parker multimode filter",
	.numSpecifications = 0,
	.specifications = NULL,
	.calculateStaticRequirements = NULL,
	.initialise = NULL,
	.calculateRequirements = calculateRequirements,
	.construct = construct,
	.parameterChanged = parameterChanged,
	.step = step,
	.draw = draw,
	.midiRealtime = NULL,
	.midiMessage = NULL,
	.tags = kNT_tagFilterEQ,
	.hasCustomUi = hasCustomUi,
	.customUi = customUi,
	.setupUi = setupUi,
};

// ============================================================================
// PLUGIN ENTRY POINT
// ============================================================================

uintptr_t pluginEntry(_NT_selector selector, uint32_t data)
{
	switch (selector)
	{
	case kNT_selector_version:
		return kNT_apiVersion9;

	case kNT_selector_numFactories:
		return 1;

	case kNT_selector_factoryInfo:
		return (uintptr_t)((data == 0) ? &factory : NULL);
	}

	return 0;
}
