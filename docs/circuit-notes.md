# DigiTech PDS 20/20 Multi-Play: circuit notes (from the factory schematics)

**Source:** Niels's scan of the DOD Electronics schematics. It has three revisions: E-1 (29 Sep 1988), E-2 and AO (4 Dec 1989). There are two sheets, analog and digital, plus a jack/switch sheet.

**Status:** this is a reading of a blurry scan. The confidence of each item is marked. Anything marked *guess* should be measured on Niels's unit before the DSP depends on it.

---

## 1. Architecture in one line

Input → pre-emphasis → sum with regen → 3-pole anti-alias LPF → **NE570 compressor** → **8-bit ADC (ADC0820)** → **64 K × 8 DRAM (2× 4464)** → **8-bit R-2R DAC (LA10502)** → **NE570 expander** → 2× 3-pole reconstruction LPF → regen tap / mix → de-emphasis on both outputs.

The delay time comes from a **voltage-controlled clock**. The buffer length is fixed per range. Turning DELAY TIME changes the sample rate, and that changes the delay. This is why sweeps bend pitch like tape, and why a held loop varispeeds.

## 2. Memory and ranges (confident about the parts, less sure of the range numbers)

- **Memory:** 2× 4464-120 DRAM (64K×4 each) = **65,536 samples of 8 bits**. The address mux is 2× 74HC157.
- **Address counter:** 74HC4040 (bits 0–11) plus a 4024 (bits 12–15). The 4024's Q1 is labelled A15, so the address names are scrambled, which doesn't matter for DRAM.
- **Range switch SW3** (two DPDT slides, legend ".125 / .500 / 2 SEC") drives lines E/F. Through U8 (74HC00), they reset the counter early:
  - **2 s:** no early reset → loop of 65,536 samples.
  - **E:** reset when net "A9" goes high → **512 samples** (if A9 really is counter bit 9).
  - **F:** reset when net "A13" goes high → **8,192 samples** (250 ms; see below).
  - **Resolved by the front panel:** the legend reads **2 SEC DELAY / 250 mSEC CHORUS-DOUBLE / 16 mSEC FLANGER**. That means 65,536 / **8,192** / **512** samples, and the ratios ×8 and ×16 fit exactly at fs_min ≈ 32.8 kHz. So the medium range resets at counter bit 13, and the pin labels on the scan are off by one bit. The short range flanges (confirmed by Niels).
- **Changing range** fires a short pulse (C35–C37 4.7 µ, diodes D6–D11) into Q7/Q11, which mutes the output while the loop length jumps.

## 3. Clock and modulation (confident about the topology, values approximate)

- **DELAY TIME (P4, 100k W):** sits between 2× 75k on +5 V, so the wiper spans about 1.5–3.5 V. Buffered by U10A.
- **LFO (U10B Schmitt + U9B integrator, C48 15 nF):** a triangle. **SPEED (P6, 500k C) + R86 2k** set the rate. Estimate f ≈ (100k/47k)/(4·R·15n) ≈ **0.07 Hz … 18 Hz** (*guess*, the op-amp pin labels look swapped on the scan). The triangle swings about ±2 V around a ~3 V reference (R82/R81).
- **R87 4.7k + C49 33 µF** after the LFO: a one-pole low-pass at about 1 Hz. **Depth shrinks as SPEED rises, and the triangle rounds off toward a sine at faster rates.** This is a characteristic quirk worth keeping.
- **WIDTH (P7, 100k W) is a crossfader, not a depth pot.** One end is the DELAY TIME voltage, the other is the (filtered) LFO voltage, and the wiper picks a blend. At full WIDTH the delay knob does nothing, and the sweep centres on the LFO's fixed ~3 V point.
- **Exponential-ish converter:** U9A → 5× 1N4148 in series (with R90 22k) → node with R91 2 M from +9 V and **P3 (50k trim) to ground** → U22B/Q12 V→I (R94 1k) → current-starved 74AC04 oscillator with C53 120 pF. The diodes give a soft knee, so the clock frequency is roughly proportional to (V − ~2.5 V). **Delay ∝ 1/f, so the taper bunches up hard at the long end.** The spec says 13:1.
- **P3 trim** sets the clock's floor and gain. Turning it down slows the clock, which is almost certainly the "up to 13 s" trick.
- **R92 10k / C52 0.1 µF** (≈ 160 Hz) smooth the control voltage before the clock, so very fast LFO or knob moves get slewed slightly.
- **Implied sample rates on the 2 s range:** 65,536 / 2 s ≈ **32.8 kHz at the slowest**, up to about 13× faster (~425 kHz) at the shortest delay (*derived*).
- **Sequencing:** 2× 74HC195 shift registers generate RAS/CAS/WE/OE and the ADC strobes from the VCO, so there are several VCO clocks per sample.

## 4. Converters and companding (confident)

- **ADC0820:** 8-bit half-flash. The DAC is an **8-bit R-2R network (RN1, LA10502)** latched by a 74HC374 and buffered by U11B.
- **NE570 (U17):** half compresses before the ADC, half expands after the DAC. That's 2:1 companding. **P2 "BIAS TRIM"** sets the THD/DC bias.
- **Pre-emphasis at the input buffer (U2A):** 33k/10k + 2.7 nF gives a shelf of **+12.3 dB, zero ≈ 1.37 kHz, pole ≈ 5.9 kHz**.
- **Matching de-emphasis at both outputs** (R23 3.3k / R24 1k + C16 27 nF; and R52/R53/C34): the mirror-image shelf, −12.3 dB.
- **What you hear:** 8-bit + compander + emphasis gives breathing and pumping noise that follows the signal, and grit on the decaying tails.

## 5. Filters (topology fairly confident, response is a guess)

- **Anti-alias:** R7/R8/R9 10k, C3 1 nF, C5 5.6 nF, C4 120 pF around a 2N5088 follower. A 3-pole Sallen-Key; modelled at about 0 dB to 5 kHz, a +2.5 dB bump near 12 kHz, −3 dB ≈ 19 kHz, steep after that.
- **Reconstruction:** two more identical sections (Q6, Q5), so the wet path is 6-pole.
- **The filters are fixed while the sample rate moves.** At the long end of the 2 s range (fs ≈ 33 kHz, Nyquist ≈ 16 kHz), content between 16 and 19 kHz aliases. A little dirt at long delays, none at short ones.
- **Every repeat passes through the whole chain again** (filters, compander, 8-bit), so the repeats darken and get grainier.

## 6. Feedback (confident)

- **REGEN (P5, 100k W)** taps the wet output (U11A) back into the input summer U2B through R5 10k + **P1 "REGEN TRIM" (50k)**. With R6 22k, the maximum loop gain is between 0.37 and **2.2** depending on P1, so **it can self-oscillate**. The ADC and compander clipping stop it running away.
- **U11A:** unity gain, inverting, with C44 1.5 nF (pole ≈ 4.8 kHz). Another darkening step per repeat. It also flips the wet polarity on each pass.

## 7. Outputs and mix (fairly confident)

- **OUTPUT 1 (U5B):** dry + wet summed 1:1 (R18 = R19 = R26 = 10k). **The MIX knob does not affect it.**
- **OUTPUT 2 (U1A):** MIX (P8, 500k C) with J113 Q8 on the wiper. It crossfades dry and wet through R47/R50 20k; gain is about 0.8.
- **Stereo comes from polarity.** Output 1 takes the wet after the inverting U11A, Output 2 before it, so the wet is **opposite in polarity on the two outputs**. Summed to mono, the wet partly cancels; in stereo it sounds wide.
- **Dry path:** the pre-emphasised input buffer, inverted by U1B, then de-emphasised at the outputs. The dry is never digitised.

## 8. Switches (confident)

- **BYPASS (SW1, momentary):** a 4007 flip-flop (U3) with LED D3. It drives Q8 J113 (the mix wiper) and Q11 to mute the wet. The dry is always live (buffered bypass).
- **REPEAT HOLD (SW2, momentary):** a 4007 flip-flop (U4) with LED D2. Line C gates the **DRAM write-enable** (U8D NAND → C2). **Hold = stop writing; the buffer loops forever.** The clock keeps running, so DELAY TIME and the LFO still change pitch and speed while held. Releasing hold resumes writing over the loop as the address passes, so with REGEN up you get sound-on-sound.

## 9. Power

- 9 V (battery or 1/8" jack), a 78L05 for the logic, and a J111/2N5088 power-on mute.

## 10. Trimmers

| Trim | Function | Stock | Mod potential |
|---|---|---|---|
| P1 | regen max | below oscillation | self-oscillation |
| P2 | NE570 bias/THD | set for minimum distortion | compander dirt |
| P3 | clock range | 2 s max | longer delays, down to about 13 s |

## 11. DSP translation (plan)

- **Fixed ring buffer** of N = 65536 / 8192 / 512 samples, **8-bit linear quantisation**, running at a **virtual sample rate** fs_v = N / delay. Read and write at the same virtual tick, read before write.
- **Varispeed** by accumulating phase at fs_v/48000 per host sample. Use band-limited interpolation into and out of the buffer (polyphase or a 4-point Hermite with an LPF that tracks fs_v). Where fs_v < host fs, the fixed analog-modelled filters let the right amount of alias through.
- **Pipeline per pass:** pre-emph → 3-pole SK LPF → 2:1 compressor (NE570 model: RMS detector ≈ 10 ms) → 8-bit quantise → buffer → expander → 6-pole LPF → regen (inverting) / outputs → de-emph.
- **Control voltage path:** delay knob voltage, WIDTH crossfade to the LFO (triangle → 1 Hz one-pole), diode-knee V→f curve, 160 Hz slew. CPU-cheap.
- **Hold** = write-enable off. **Bypass** = wet mute, dry always live; tails optional (as in Taj Mahal).
- **Outputs:** out1 = dry + wet, out2 = mix(dry, −wet) after de-emphasis.

---

## 12. Niels's mods and decisions (2 Oct 2026)

1. **SLAM, as a surface knob.** On the hardware, a momentary footswitch overrode REGEN with a trimpot value set past oscillation, so it built up into self-oscillation.
   - **Plugin:** a front-panel `slam_level` knob next to REGEN, and a `slam` footswitch that **switches between the two values**.
   - Default to momentary (`mod:preferMomentaryOnByDefault`); it can be re-addressed as latching.
   - Use a ~5 ms smoother. The range goes up to about 2.2× loop gain, so it can run away into the compander/8-bit clipping.
2. **Clock-floor trim (P3) set to about 4 s on the long range → an ORIGINAL / DOUBLED toggle** (the same idea as the EC-280's 300/600 switch and Taj Mahal's Vintage toggle).
   - **Plugin:** a `stretch` toggle on the face. DOUBLED halves the clock on every range: 4 s / 500 ms / 32 ms. The flanger range then becomes chorus-ish, with a minimum of about 2.5 ms.
   - At 4 s the slowest virtual fs is ≈ 16.4 kHz, so the fixed ~19 kHz filters let more alias through. Keep it.
   - **The face's range legend swaps text with the toggle** (2 SEC / 250 mSEC / 16 mSEC ↔ 4 SEC / 500 mSEC / 32 mSEC). This uses modgui custom JavaScript (see §13).
3. **Effects loop in the feedback path: deferred** (never built on the pedal, not essential). If revisited: send = wet at the regen tap, return = into the input summer. Whether mod-ui accepts the graph cycle still needs testing.

## 13. Face trick: text that changes with a control (verified in mod-ui source)

**The mechanism is checked; it hasn't been tried on the Duo yet.** Add it to HANDOVER §7 once it ships.

- **Declare a script:** `modgui:javascript <modgui/script.js>` in modgui.ttl. mod-ui fetches the file and runs `eval('method = ' + code)`, so the file must be a **single anonymous function**: `function (event, funcs) { … }`.
- **Events:**
  - `{type:'start', ports:[{symbol,value}…], parameters}` fires once when the face is ready.
  - `{type:'change', symbol, value}` fires on every port change, whether it comes from the UI, a footswitch, MIDI or a preset.
  - Every event carries `event.icon` (the jQuery root of the face), `event.data` (persistent per-instance storage) and `api_version 3`.
- **For the range legend:**

  ```js
  function (event) {
    function set(v) { event.icon.find('.mp-legend').toggleClass('doubled', v > 0.5); }
    if (event.type == 'start') event.ports.forEach(function (p) { if (p.symbol == 'stretch') set(p.value); });
    else if (event.type == 'change' && event.symbol == 'stretch') set(event.value);
  }
  ```

  CSS shows one of two legend images (or text spans) depending on `.doubled`.
- **If the script throws, mod-ui silently disables it** (it logs "javascript code is broken"). Test in a browser preview first.
- **The Duo's own screen** shows the TTL `lv2:scalePoint` labels for an addressed range port. Those are static, so name them neutrally ("Delay / Chorus / Flanger").
