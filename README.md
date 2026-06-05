# PTZ Controller

An OBS Studio plugin for controlling PTZ cameras, with a re-imagined control
dock and **automatic protocol detection** — point it at a camera (or scan the
network) and it figures out how to talk to it, instead of making you hand-enter
host/port/protocol.

A clean-room alternative to [glikely/obs-ptz](https://github.com/glikely/obs-ptz),
focused on a streamlined UI and zero-config camera setup.

## Highlights

- **Auto-detect** — Probe a host or scan your whole subnet; the plugin fires
  each protocol's identity query and detects which one the camera speaks
  (VISCA-over-IP Sony framing, raw VISCA over UDP/TCP, or NDI), then configures
  it for you.
- **Protocols**
  - VISCA over IP — Sony framing (UDP :52381), **raw VISCA** (UDP :1259), and
    raw VISCA over **TCP :5678** (PTZOptics and many others)
  - **NDI PTZ** — control over the NDI stream itself (great for NDI cameras like
    OBSBOT); NDI source discovery built in
- **Control dock** — pan/tilt pad with home, zoom & focus rockers, autofocus,
  a speed slider, and a collapsible preset bank.
- **Cameras** — quick-select buttons across the top to switch the controlled
  camera; add/rename/remove in Settings.
- **Presets** — right-click any preset to Save / Rename / Clear; named presets
  show their label. Saved per camera.

## Install (macOS)

A signed/notarized package will be published on the
[Releases](https://github.com/preppdev/obs-ptzcontroller/releases) page. Until
then, build locally (below). Requires OBS Studio 30+; the NDI backend uses the
NDI runtime if it's installed (it's optional).

## Build

Out-of-tree against an installed libobs + Qt 6, or via the OBS plugin template.
On macOS the repo includes helper scripts:

- `verify-build.sh` — quick compile/link check
- `pack-macos.sh` — build + install into your local OBS for testing
- `package-macos.sh` — build a distributable (signed/notarized) `.pkg` + `.zip`

The NDI backend compiles against the NDI SDK headers and loads the NDI runtime
dynamically at runtime (no hard link dependency).

## Layout

```
src/plugin-main.cpp        module entry, registers the dock
src/ptz-device.hpp         abstract PTZ device + config
src/visca-ip.cpp/.hpp      VISCA over IP (Sony UDP / raw UDP / raw TCP)
src/ndi-runtime.cpp/.hpp   dynamic NDI runtime loader
src/ndi-device.cpp/.hpp    NDI PTZ device + NDI source discovery
src/ptz-probe.cpp/.hpp     protocol auto-detect (host probe + subnet scan)
src/ptz-manager.cpp/.hpp   device registry + persistence
src/dock.cpp/.hpp          the control dock + settings/add dialogs
```

## License

GNU General Public License v2.0 or later (GPL-2.0-or-later) — see
[LICENSE](LICENSE), matching OBS Studio / libobs.

## Credits

Clean-room re-implementation inspired by
[glikely/obs-ptz](https://github.com/glikely/obs-ptz) (also GPL-2.0). VISCA and
NDI control built on the published protocol specs and the NDI SDK.
