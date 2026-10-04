# Development

How MultiPlay is built, tested and changed. The same process applies to every New Horizon plugin.

## Toolchain

```sh
apt-get install g++ g++-arm-linux-gnueabihf g++-aarch64-linux-gnu qemu-user lilv-utils
pip install numpy scipy pillow playwright
gcc -O2 -Idpf/distrho/src -Idpf/distrho/src/lv2 tools/lv2host.c -o tools/lv2host -ldl
```

## The loop for every change

1. **Decide the behaviour first.** Faithful to the original by default; any deliberate difference goes in the CHANGELOG.
2. **Edit the sources** (`plugins/`, `bundle/`). The LV2 metadata is hand-written: keep port order, ranges and the URI in step with `DistrhoPluginInfo.h` and `initParameter()`.
3. **Build three ways:** native (`make`), Duo (`CXX=arm-linux-gnueabihf-g++ CXXFLAGS="-mcpu=cortex-a7 -mfpu=neon-vfpv4 -mfloat-abi=hard" make`), Duo X / Dwarf (`CXX=aarch64-linux-gnu-g++ make`). Warnings are bugs.
4. **Check the metadata:** build DPF's `lv2_ttl_generator` and compare its output port by port with `bundle/nhe-multiplay.lv2/nhe-multiplay.ttl`; then `LV2_PATH=bin lv2info https://github.com/Kiwooky/NHE-MultiPlay2020`.
5. **Run the audio suite:** `python3 tools/mptest.py` natively, then the ARM builds under qemu:
   `MP_SO=<arm .so> MP_HOST="qemu-arm -L /usr/arm-linux-gnueabihf <arm lv2host>" python3 tools/mptest.py`.
   It checks delay times per range, levels, Hold varispeed, Slam build-up and release, bypass (clicks, dry at unity, held loop survives), range switching, a max-everything torture run at 44.1/48/96 kHz and silence. **Every reported bug gets a test that fails first.**
6. **Face changes:** test the template and `script-multiplay.js` against mod-ui's own `html/js/modgui.js` and its jQuery 1.9.1 in headless Chromium, clicking with a little mouse wobble. Then `make && python3 tools/render.py && make` to refresh the screenshot and thumbnail.
7. **Bump the version** in `getVersion()` and the TTL together (`d_version(1,0,N)` ↔ `lv2:minorVersion 2 ; lv2:microVersion N`). MOD caches plugin data per version: an unchanged version shows stale faces.
8. **Commit, update `NHE_MULTIPLAY_VERSION`** in the package file to the new commit, build on builder.mod.audio and check on hardware: face, sound, footswitches, bypass, CPU meter.

## Rules learned the hard way

- **DPF needs `opts:options` and `urid:map`** declared as required features.
- **Parameters arrive with the first `run()`**, not before `activate()`: snap smoothers to their targets on the first run.
- **Buttons and footswitches use `mod-widget="switch"`.** The default film widget ignores a click if the mouse moves a couple of pixels.
- **The face script** (`modgui:javascript`) is one anonymous `function (event, funcs)`. `funcs.set_port_value()` sets controls; if it throws, mod-ui silently disables it.
- **Once public, ports are frozen.** Never remove, reorder or rename ports, and never change the URI: saved pedalboards depend on them.
