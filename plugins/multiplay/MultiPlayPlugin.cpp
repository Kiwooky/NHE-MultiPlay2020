#include "DistrhoPlugin.hpp"

#include <cmath>
#include <cstring>
#include <cstdint>

START_NAMESPACE_DISTRHO

// ---------------------------------------------------------------------------
// MultiPlay 20/20 - a model of a 1988 variable-clock 8-bit digital delay.
//
// The original stores 64K 8-bit samples in DRAM. The DELAY TIME knob does not
// move a tap: it changes the speed of the clock that walks through a loop of
// fixed length (512, 8192 or 65536 samples, set by the range switch). So the
// delay time is loop length / clock rate, sweeps bend pitch like tape, and a
// frozen loop (Repeat Hold = write disabled) varispeeds when you turn the knob.
//
// Here the converters run at that "virtual" clock rate inside the host-rate
// signal path. Everything analog (emphasis, filters, NE570 compander, mixing)
// runs at the host rate.
// ---------------------------------------------------------------------------

static const float kPi        = 3.14159265358979f;
static const float kClockMin  = 32768.0f;       // Hz: 65536 samples / 2 s
static const float kClockSpan = 12.0f;          // 13:1 sweep
static const uint32_t kMemSize = 65536;         // 2x 4464 DRAM, 8 bits

static inline float clampf(float x, float lo, float hi)
{
    return x < lo ? lo : (x > hi ? hi : x);
}

static inline float onePoleCoef(float hz, float sr)
{
    return 1.0f - std::exp(-2.0f * kPi * hz / sr);
}

// Rational tanh approximation; |x| < 3 accurate, saturates beyond.
static inline float softClip(float x)
{
    if (x >  3.0f) return  1.0f;
    if (x < -3.0f) return -1.0f;
    const float x2 = x * x;
    return x * (27.0f + x2) / (27.0f + 9.0f * x2);
}

// First-order section from an analog prototype via the bilinear transform.
struct FirstOrder
{
    float b0 = 1.0f, b1 = 0.0f, a1 = 0.0f, x1 = 0.0f, y1 = 0.0f;

    // low-pass, prewarped at fc
    void setLowpass(float fc, float sr)
    {
        fc = clampf(fc, 10.0f, 0.45f * sr);
        const float k = std::tan(kPi * fc / sr);
        b0 = b1 = k / (1.0f + k);
        a1 = (k - 1.0f) / (k + 1.0f);
    }
    // shelf H(s) = (1 + s*tz) / (1 + s*tp)
    void setShelf(float tz, float tp, float sr)
    {
        const float K = 2.0f * sr;
        const float n = 1.0f + K * tp;
        b0 = (1.0f + K * tz) / n;
        b1 = (1.0f - K * tz) / n;
        a1 = (1.0f - K * tp) / n;
    }
    inline float process(float x)
    {
        const float y = b0 * x + b1 * x1 - a1 * y1;
        x1 = x; y1 = y;
        return y;
    }
    void clear() { x1 = y1 = 0.0f; }
};

// Second-order low-pass (RBJ form = bilinear, prewarped at f0).
struct Biquad
{
    float b0 = 1.0f, b1 = 0.0f, b2 = 0.0f, a1 = 0.0f, a2 = 0.0f;
    float z1 = 0.0f, z2 = 0.0f;

    void setLowpass(float f0, float q, float sr)
    {
        f0 = clampf(f0, 10.0f, 0.45f * sr);
        const float w = 2.0f * kPi * f0 / sr;
        const float c = std::cos(w), s = std::sin(w);
        const float alpha = s / (2.0f * q);
        const float a0 = 1.0f + alpha;
        b0 = (1.0f - c) * 0.5f / a0;
        b1 = (1.0f - c) / a0;
        b2 = b0;
        a1 = -2.0f * c / a0;
        a2 = (1.0f - alpha) / a0;
    }
    inline float process(float x)
    {
        const float y = b0 * x + z1;
        z1 = b1 * x - a1 * y + z2;
        z2 = b2 * x - a2 * y;
        return y;
    }
    void clear() { z1 = z2 = 0.0f; }
};

// 3-pole Sallen-Key (10k / 1n / 5.6n / 120p around a 2N5088 follower):
// complex pair 15.0 kHz Q 1.39, real pole 26.7 kHz.
struct SallenKey3
{
    Biquad bq;
    FirstOrder p;
    void set(float sr)
    {
        bq.setLowpass(14980.0f, 1.389f, sr);
        p.setLowpass(26733.0f, sr);
    }
    inline float process(float x) { return p.process(bq.process(x)); }
    void clear() { bq.clear(); p.clear(); }
};

// ---------------------------------------------------------------------------

class MultiPlayPlugin : public Plugin
{
public:
    MultiPlayPlugin()
        : Plugin(kParameterCount, 0, 0)
    {
        fParams[kDelayTime] = 6.0f;
        fParams[kWidth]     = 0.0f;
        fParams[kSpeed]     = 3.0f;
        fParams[kRegen]     = 4.0f;
        fParams[kMix]       = 5.0f;
        fParams[kRange]     = 2.0f;
        fParams[kSlamLevel] = 7.0f;
        fParams[kTimeMod]   = 0.0f;
        fParams[kHold]      = 0.0f;
        fParams[kSlam]      = 0.0f;
        fParams[kRampTime]  = 0.45f;
        fParams[kBypass]    = 0.0f;

        std::memset(fMem, 0, sizeof(fMem));
        activate();
    }

protected:
    const char* getLabel()       const override { return "MultiPlay"; }
    const char* getDescription() const override { return "Variable-clock 8-bit delay, flanger and looper modelled on a 1988 multi-function digital delay."; }
    const char* getMaker()       const override { return "New Horizon Electronics"; }
    const char* getHomePage()    const override { return "https://github.com/Kiwooky/NHE-MultiPlay2020"; }
    const char* getLicense()     const override { return "MIT"; }
    uint32_t    getVersion()     const override { return d_version(1, 0, 0); }
    int64_t     getUniqueId()    const override { return d_cconst('M', 'P', '2', '0'); }

    void initParameter(uint32_t index, Parameter& p) override
    {
        p.hints = kParameterIsAutomatable;
        p.ranges.min = 0.0f;
        p.ranges.max = 10.0f;

        switch (index) {
        case kDelayTime:
            p.name = "Delay Time"; p.symbol = "delay_time"; p.ranges.def = 6.0f;
            break;
        case kWidth:
            p.name = "Width"; p.symbol = "width"; p.ranges.def = 0.0f;
            break;
        case kSpeed:
            p.name = "Speed"; p.symbol = "speed"; p.ranges.def = 3.0f;
            break;
        case kRegen:
            p.name = "Regen"; p.symbol = "regen"; p.ranges.def = 4.0f;
            break;
        case kMix:
            p.name = "Mix"; p.symbol = "mix"; p.ranges.def = 5.0f;
            break;
        case kRange: {
            p.hints |= kParameterIsInteger;
            p.name = "Range"; p.symbol = "range";
            p.ranges.max = 2.0f; p.ranges.def = 2.0f;
            static ParameterEnumerationValue values[3];
            values[0].value = 0.0f; values[0].label = "Flange";
            values[1].value = 1.0f; values[1].label = "Chorus";
            values[2].value = 2.0f; values[2].label = "Echo";
            p.enumValues.count = 3;
            p.enumValues.restrictedMode = true;
            p.enumValues.values = values;
            p.enumValues.deleteLater = false;
            break;
        }
        case kSlamLevel:
            p.name = "Slam Regen"; p.symbol = "slam_level"; p.ranges.def = 7.0f;
            break;
        case kTimeMod:
            p.hints |= kParameterIsInteger | kParameterIsBoolean;
            p.name = "Time Mod"; p.symbol = "time_mod";
            p.ranges.max = 1.0f; p.ranges.def = 0.0f;
            break;
        case kHold:
            p.hints |= kParameterIsInteger | kParameterIsBoolean;
            p.name = "Repeat Hold"; p.symbol = "hold";
            p.ranges.max = 1.0f; p.ranges.def = 0.0f;
            break;
        case kSlam:
            p.hints |= kParameterIsInteger | kParameterIsBoolean;
            p.name = "Slam"; p.symbol = "slam";
            p.ranges.max = 1.0f; p.ranges.def = 0.0f;
            break;
        case kRampTime:
            p.hints |= kParameterIsLogarithmic;
            p.name = "Ramp Swell Time"; p.symbol = "ramp_time"; p.unit = "s";
            p.ranges.min = 0.3f; p.ranges.max = 3.0f; p.ranges.def = 0.45f;
            break;
        case kBypass:
            p.initDesignation(kParameterDesignationBypass);
            break;
        }
    }

    float getParameterValue(uint32_t index) const override
    {
        return (index < kParameterCount) ? fParams[index] : 0.0f;
    }

    void setParameterValue(uint32_t index, float value) override
    {
        if (index < kParameterCount) fParams[index] = value;
    }

    void activate() override
    {
        fSr = (float)getSampleRate();
        if (fSr <= 0.0f) fSr = 48000.0f;

        // tau = 1 + 33k/(10k + 1/sC), C = 2.7 nF  ->  (1 + s*116.1us)/(1 + s*27us)
        fEmph.setShelf(116.1e-6f, 27.0e-6f, fSr);
        fDeemph1.setShelf(27.0e-6f, 116.1e-6f, fSr);
        fDeemph2.setShelf(27.0e-6f, 116.1e-6f, fSr);
        fAA.set(fSr);
        fRecon1.set(fSr);
        fRecon2.set(fSr);
        fU11.setLowpass(4823.0f, fSr);          // 22k || 1.5 nF

        fEnvCoef   = onePoleCoef(33.9f, fSr);   // NE570 rectifier, 10k * 0.47 uF
        fLfoLpCoef = onePoleCoef(1.03f, fSr);   // R87 4.7k into C49 33 uF
        fCvCoef    = onePoleCoef(160.0f, fSr);  // R92 10k / C52 0.1 uF
        fFast      = onePoleCoef(1.0f / (2.0f * kPi * 0.005f), fSr);  // 5 ms
        fGlide     = onePoleCoef(1.0f / (2.0f * kPi * 0.030f), fSr);  // 30 ms

        clearState();

        const bool bypassed = fParams[kBypass] > 0.5f;
        fWetGain = bypassed ? 0.0f : 1.0f;

        fRangeIdx  = rangeIndex();
        fLen       = kLens[fRangeIdx];
        fRangeGain = 1.0f;
        fRangeState = 0;

        fStretch = fParams[kTimeMod] > 0.5f ? 0.5f : 1.0f;
        fFirstRun = true;
        fRegenS = regenGain(); fSlamS = slamGain(); fSlamX = slamTarget();
        fCv = cvTarget(0.0f);
    }

    void clearState()
    {
        std::memset(fMem, 0, sizeof(fMem));
        fPos = 0; fPhase = 0.0f; fDac = 0.0f; fAdcPrev = 0.0f;
        fEmph.clear(); fDeemph1.clear(); fDeemph2.clear();
        fAA.clear(); fRecon1.clear(); fRecon2.clear(); fU11.clear();
        fEnvC = fEnvE = 0.01f;
        fFb = 0.0f;
        fLfoPhase = 0.25f; fLfoLp = 0.0f;
    }

    void run(const float** inputs, float** outputs, uint32_t frames) override
    {
        const float* in   = inputs[0];
        float*       out1 = outputs[0];
        float*       out2 = outputs[1];
        const float  sr   = fSr;

        // --- controls ------------------------------------------------------
        const float width  = clampf(fParams[kWidth] / 10.0f, 0.0f, 1.0f);
        const float lfoInc = 0.07f * std::pow(253.0f, clampf(fParams[kSpeed] / 10.0f, 0.0f, 1.0f)) / sr;
        const float gRegen = regenGain(), gSlam = slamGain(), xTarget = slamTarget();

        // Slam swell: one-pole reaching ~95 % in the Ramp Swell Time
        // (tau = time / 3); the fall back to Regen takes twice as long.
        const float ramp = clampf(fParams[kRampTime], 0.3f, 3.0f);
        if (ramp != fRampCached) {
            fRampCached = ramp;
            fSlamUp   = 1.0f - std::exp(-3.0f / (ramp * sr));
            fSlamDown = 1.0f - std::exp(-3.0f / (2.0f * ramp * sr));
        }
        const float stretchTarget = fParams[kTimeMod] > 0.5f ? 0.5f : 1.0f;
        const bool  hold   = fParams[kHold] > 0.5f;

        // MIX (both outputs; on the original it only worked on out 2):
        // dry and wet both at full level around noon, each
        // fading out over its own half of the knob.
        const float m = clampf(fParams[kMix] / 10.0f, 0.0f, 1.0f);
        const float mixDry = clampf(2.0f * (1.0f - m), 0.0f, 1.0f);
        const float mixWet = clampf(2.0f * m, 0.0f, 1.0f);

        // Bypass as on the original: the effect output is muted (J113 on the
        // mix), but the delay keeps running underneath. Repeats carry on and a
        // held loop keeps playing in the background, so it is there again the
        // moment the effect is switched back on. Dry always passes at unity.
        const bool bypassed = fParams[kBypass] > 0.5f;
        const float wetTarget = bypassed ? 0.0f : 1.0f;

        // range switch: mute, change the loop length, fade back in
        const int newRange = rangeIndex();
        if (newRange != fRangeIdx && fRangeState == 0) fRangeState = 1;

        if (fFirstRun) {
            // controls arrive with the first run(): start there, no glides
            fFirstRun = false;
            fRegenS = gRegen; fSlamS = gSlam; fSlamX = xTarget;
            fStretch = stretchTarget;
            fWetGain = wetTarget;
            fRangeIdx = newRange;
            fLen = kLens[fRangeIdx];
            fRangeState = 0;
            fRangeGain = 1.0f;
            fCv = cvTarget(width);
        }

        const float kLr = 0.35f;            // compander reference level
        const float invLr = 1.0f / kLr;

        for (uint32_t i = 0; i < frames; ++i) {
            const float x = in[i];

            fWetGain += fFast * (wetTarget - fWetGain);
            // knobs follow quickly; the Slam footswitch crossfades between
            // them with a swell in (~0.45 s) and a slower fall back (~0.9 s)
            fRegenS += fFast * (gRegen - fRegenS);
            fSlamS  += fFast * (gSlam - fSlamS);
            fSlamX  += (xTarget > fSlamX ? fSlamUp : fSlamDown) * (xTarget - fSlamX);
            const float loopGain = fRegenS + fSlamX * (fSlamS - fRegenS);
            fStretch += fGlide * (stretchTarget - fStretch);

            if (fRangeState == 1) {                  // fading out
                fRangeGain -= 1.0f / (0.003f * sr);
                if (fRangeGain <= 0.0f) {
                    fRangeGain = 0.0f;
                    fRangeIdx = rangeIndex();
                    fLen = kLens[fRangeIdx];
                    if (fPos >= fLen) fPos %= fLen;
                    fRangeState = 2;
                }
            } else if (fRangeState == 2) {           // muted, fading in (~40 ms)
                fRangeGain += 1.0f / (0.040f * sr);
                if (fRangeGain >= 1.0f) { fRangeGain = 1.0f; fRangeState = 0; }
            }

            // --- clock control voltage ------------------------------------
            fLfoPhase += lfoInc;
            if (fLfoPhase >= 1.0f) fLfoPhase -= 1.0f;
            const float tri = fLfoPhase < 0.5f ? 4.0f * fLfoPhase - 1.0f
                                               : 3.0f - 4.0f * fLfoPhase;
            fLfoLp += fLfoLpCoef * (tri - fLfoLp);
            fCv += fCvCoef * (cvTarget(width) - fCv);

            const float c = clampf(fCv, 0.0f, 1.3f);
            const float ratio = 1.0f + kClockSpan * std::pow(c, 1.3f);
            const float inc = kClockMin * fStretch * ratio / sr;   // ticks per host sample

            // --- analog front end ----------------------------------------
            const float xe = fEmph.process(x);
            float s = xe + fFb;
            s = 1.2f * softClip(s * (1.0f / 1.2f));                 // TL062 summer headroom
            const float a = fAA.process(s);

            // NE570 compressor (2:1). The rectifier senses the OUTPUT, so the
            // expander, which senses its own input, tracks it exactly and the
            // pair cancels even during attacks (apart from 8-bit and clipping).
            fNoise = fNoise * 1664525u + 1013904223u;
            const float hiss = (float)(int32_t)fNoise * (2.0e-4f / 2147483648.0f);
            const float envC = fEnvC > 1e-3f ? fEnvC : 1e-3f;
            const float y = clampf((a + hiss) * kLr / envC, -1.5f, 1.5f);
            fEnvC += fEnvCoef * (std::fabs(y) - fEnvC);

            // --- converters at the virtual clock ---------------------------
            const bool write = !hold && fRangeState != 1;
            float tLeft = 1.0f, acc = 0.0f;
            for (;;) {
                float tt = (1.0f - fPhase) / inc;
                if (tt < 0.0f) tt = 0.0f;
                if (tt >= tLeft) {
                    acc += fDac * tLeft;
                    fPhase += tLeft * inc;
                    break;
                }
                acc += fDac * tt;
                tLeft -= tt;
                fPhase = 0.0f;

                const float frac = 1.0f - tLeft;
                const float adcIn = fAdcPrev + (y - fAdcPrev) * frac;

                fDac = (float)fMem[fPos] * (1.0f / 127.0f);
                if (write) {
                    const float q = clampf(adcIn, -1.0f, 1.0f) * 127.0f;
                    fMem[fPos] = (int8_t)std::lrint(q);
                }
                if (++fPos >= fLen) fPos = 0;
            }
            fAdcPrev = y;
            const float d = acc;

            // NE570 expander (1:2)
            fEnvE += fEnvCoef * (std::fabs(d) - fEnvE);
            float z = d * fEnvE * invLr;
            z = softClip(z);                         // U17A / TL062 output swing

            const float wPre = fRecon2.process(fRecon1.process(z));
            const float wU11 = fU11.process(wPre);   // U11A (inverting; sign folded into the sums)

            fFb = loopGain * wU11 + 1e-18f;

            // --- outputs ---------------------------------------------------
            const float wg = fWetGain * fRangeGain;
            const float w1 = fDeemph1.process(wU11) * wg;
            const float w2 = fDeemph2.process(wU11) * wg;   // out 2 also after the U11A treble filter (smoother than stock)
            // bypassed: the dry fades up to unity whatever MIX is set to
            const float dry = 1.0f + fWetGain * (mixDry - 1.0f);
            out1[i] = dry * x + mixWet * w1;         // OUTPUT 1: MIX, smoother wet
            out2[i] = dry * x - mixWet * w2;         // OUTPUT 2: MIX, wet inverted
        }

    }

private:
    static constexpr uint32_t kLens[3] = { 512, 8192, 65536 };

    int rangeIndex() const
    {
        const int r = (int)(fParams[kRange] + 0.5f);
        return r < 0 ? 0 : (r > 2 ? 2 : r);
    }

    float regenGain() const { return 1.0f * clampf(fParams[kRegen] / 10.0f, 0.0f, 1.0f); }
    float slamGain()  const { return 2.0f * clampf(fParams[kSlamLevel] / 10.0f, 0.0f, 1.0f); }
    float slamTarget() const { return fParams[kSlam] > 0.5f ? 1.0f : 0.0f; }

    // Clock control in normalised units: 0 = slowest clock (longest delay),
    // 1 = 13x faster. WIDTH crossfades the DELAY TIME voltage with the
    // (low-passed) LFO, which sits at its own fixed centre.
    float cvTarget(float width) const
    {
        const float k = clampf(fParams[kDelayTime] / 10.0f, 0.0f, 1.0f);
        const float cKnob = 1.0f - k;
        const float cLfo  = 0.75f + 0.6f * fLfoLp;
        return (1.0f - width) * cKnob + width * cLfo;
    }

    float fParams[kParameterCount];

    int8_t   fMem[kMemSize];
    uint32_t fPos = 0, fLen = 65536;
    float    fPhase = 0.0f, fDac = 0.0f, fAdcPrev = 0.0f;

    FirstOrder fEmph, fDeemph1, fDeemph2, fU11;
    SallenKey3 fAA, fRecon1, fRecon2;

    float fSr = 48000.0f;
    float fEnvCoef = 0.0f, fLfoLpCoef = 0.0f, fCvCoef = 0.0f, fFast = 0.0f, fGlide = 0.0f;
    float fSlamUp = 0.0f, fSlamDown = 0.0f, fRampCached = -1.0f, fRegenS = 0.0f, fSlamS = 0.0f, fSlamX = 0.0f;
    float fEnvC = 0.01f, fEnvE = 0.01f, fFb = 0.0f;
    float fLfoPhase = 0.25f, fLfoLp = 0.0f, fCv = 0.0f;
    float fWetGain = 1.0f, fStretch = 1.0f;
    float fRangeGain = 1.0f;
    int   fRangeIdx = 2, fRangeState = 0;
    uint32_t fNoise = 22222u;
    bool  fFirstRun = true;

    DISTRHO_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MultiPlayPlugin)
};

constexpr uint32_t MultiPlayPlugin::kLens[3];

Plugin* createPlugin() { return new MultiPlayPlugin(); }

END_NAMESPACE_DISTRHO
