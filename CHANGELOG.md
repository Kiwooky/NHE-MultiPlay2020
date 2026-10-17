# Changelog

## 1.0.2 — 2026-10-17

- New pedal face at 2x resolution (sharp on retina screens and tablets). The footswitches press
  down while held and spring back; the LED beside each one shows its state. When a hardware footswitch
  or MIDI changes them, they flash down briefly. The Time Mod button latches: it stays down while
  Time Mod is on.
- New setting **Output 2**: Inverted (original, cancels in mono), Normal (mono-safe, no width),
  Offset (echoes trail by 12 ms: wide and mono-safe). For stereo rigs that get summed to mono at venues.
  Measured with both outputs summed: Inverted leaves no echo, Normal is exactly twice Output 1,
  Offset keeps the echoes (+2.8 dB over Output 1 on a test note); switching modes doesn't click.
- Controls grouped in the settings as Delay, Modulation, Mix and Expression (LV2 port groups).
- The new setting is added after the existing ports, so saved pedalboards and presets keep working;
  they load with Output 2 on Inverted, as before.

## 1.0.1 — 2026-10-04

- Ten factory presets by Niels: Choro-trem, Flangy flangeface, Haunting detunes, Inception,
  Muted palm tapestops, Short clean loops, Slamming bliss, Triller shimmer, Uneasy slapback,
  Warbled echo. Footswitch states are not stored, so loading a preset never freezes or slams.

## 1.0.0 — 2026-10-04

First release under its own identity (URI `https://github.com/Kiwooky/NHE-MultiPlay2020`,
maker New Horizon Electronics). Carries everything from the cookbook prototype (v1.0.0–1.0.7):

- Variable-clock 8-bit delay model from the factory schematics: 2 s / 250 ms / 16 ms ranges,
  13:1 Delay Time sweep, Width/Speed LFO, NE570 compander, 8-bit grain, darkening repeats.
- Repeat Hold freezes the loop; Delay Time and the LFO still varispeed it.
- Slam footswitch and Slam Regen knob, with an adjustable Ramp Swell Time (0.3–3 s).
- Time Mod doubles every range (4 s / 500 ms / 32 ms); the face legend swaps to match.
- Bypass as on the original: echoes muted, delay and held loop keep running, dry at unity.
- Mix works on both outputs; Output 2 carries the echoes in opposite polarity for stereo width.
- Pedal face: click a range on the lever legend to select it; switch widgets for all buttons.
