<p align="center">
  <img src="ps5/sce_sys/icon0.png" alt="PSBox Cloud Gaming" width="160">
</p>

<h1 align="center">PSBox Cloud Gaming for Windows</h1>
<p align="center"><em>Xbox Cloud Gaming as a native Windows app.</em></p>

<p align="center">
  <a href="https://github.com/T0PGLitch3r/xbox-cloud-gaming-ps5/releases/latest"><img alt="release" src="https://img.shields.io/github/v/release/T0PGLitch3r/xbox-cloud-gaming-ps5?color=brightgreen"></a>
  <a href="#installation"><img alt="platform: Windows 10/11" src="https://img.shields.io/badge/platform-Windows%2010%20%7C%2011-0078d4"></a>
  <img alt="stream: 1080p60" src="https://img.shields.io/badge/stream-1080p60%20H.264-5c2d91">
  <a href="LICENSE"><img alt="license" src="https://img.shields.io/github/license/T0PGLitch3r/xbox-cloud-gaming-ps5"></a>
</p>

A Windows port of [PSBox Cloud Gaming](https://github.com/RafaelNGP/psbox-cloud-gaming)
by RafaelNGP: browse the Xbox Cloud Gaming catalog and stream games at
1080p60 from a small, fast native app, with your controller or keyboard.

- **Sign in on your phone** with a code shown on screen.
- **Game Pass and Your games** tabs, search, and free-to-play games.
- **My consoles**: play your own Xbox (Remote Play), its screen, games and apps.
- **GPU video decoding** (Direct3D 11), with the CPU as a fallback.
- **Sharp picture**: upscaling (AMD FSR 1 or Anime4K) on GPUs with Vulkan 1.3.
- **Controllers**: Xbox, PlayStation and other pads, or the keyboard.
- **In-game menu**: statistics, picture options, stream resolution.
- **Reconnects** by itself after a short network drop.
- Six languages.

You need a Microsoft account, with Game Pass Ultimate for most of the catalog.

## What this fork adds

- **Windows app**: a portable folder (`psbox.exe`, `SDL3.dll`, `assets`),
  nothing to install. Sign-in and settings are kept in
  `%LOCALAPPDATA%\psbox`.
- **Automatic updates**: the app checks for a new version each time it
  starts, installs it and restarts. Each update is signed, and the old files
  are put back if an update is cut short.
- **Always up to date with the original**: every 6 hours, a GitHub Actions job
  ([windows-release.yml](.github/workflows/windows-release.yml)) looks for a
  new upstream release, builds the Windows version and publishes it here.
- **Themes**: Settings has *Accent colour* (presets or any colour from a
  colour wheel), *Background* (colour themes, an accent glow, or a JPEG / PNG
  of your own) and *Game art on the home screen* on or off.
- **Xbox One mode** (experimental): `PSBox as Xbox One.bat` tells Xbox Cloud
  Gaming the app is an Xbox One console instead of a PC. Games that are refused
  with "DeviceTypeNotAllowed" stay refused, because that check is based on how
  you signed in.

## Installation

1. Download `PSBox-<version>-windows-x86_64.zip` from the
   [latest release](https://github.com/T0PGLitch3r/xbox-cloud-gaming-ps5/releases/latest).
2. Extract it somewhere you can write to (Desktop, Documents, a games folder),
   not under Program Files, so updates can install.
3. Run `psbox.exe`. The app isn't code-signed, so Windows SmartScreen may
   warn about it: choose **More info** → **Run anyway**.
4. Sign in: open <https://www.microsoft.com/link> on your phone or computer
   (or scan the QR code) and enter the code shown in the app.

Windows Firewall may ask about network access the first time: the app's
*Web Control* page (use your phone as a remote or keyboard) listens on your
home network. You can cancel if you don't use it.

Updates install themselves. If something goes wrong, the log is
`%LOCALAPPDATA%\psbox\xcloud.log`. Never share `account.json` from that
folder: it holds your sign-in.

## Building

The Windows app is cross-built on Linux with
[llvm-mingw](https://github.com/mstorsjo/llvm-mingw), SDL3's mingw package
(`SDL3-devel-*-mingw`) and
[Vulkan-Headers](https://github.com/KhronosGroup/Vulkan-Headers). The source is
on the [`windows`](https://github.com/T0PGLitch3r/xbox-cloud-gaming-ps5/tree/windows)
branch:

```bash
git clone --recursive --branch windows https://github.com/T0PGLitch3r/xbox-cloud-gaming-ps5.git
cd xbox-cloud-gaming-ps5
cmake -S . -B build-win -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo \
      -DCMAKE_TOOLCHAIN_FILE=cmake/windows-mingw.toolchain.cmake \
      -DCMAKE_PREFIX_PATH=/path/to/SDL3-x.y.z/x86_64-w64-mingw32 \
      -DXC_VULKAN_HEADERS=/path/to/Vulkan-Headers/include \
      -DXCLOUD_DESKTOP_APP=ON
ninja -C build-win psbox
XC_VERSION=v1.7.2 tools/desktop/package-windows.sh build-win dist   # -> dist/PSBox-v1.7.2-windows-x86_64.zip
wine build-win/psbox.exe --selftest out                              # optional: the built-in self-test
```

Requirements: CMake 3.20+, Ninja, Python 3 and curl. FFmpeg is configured and
built the first time CMake runs. For a build that updates itself, add
`-DXC_WINDOWS_UPDATE_REPO=<owner>/<repo>`, and sign each release zip with
`tools/desktop/windows-release.sh` using the private half of
`windows/release-key.pem` (the `WINDOWS_RELEASE_KEY` secret in Actions).

## License

GPL-3.0-or-later; see `LICENSE`. All bundled components are compatible
(SDL3, Mbed TLS, libdatachannel, FFmpeg under the LGPL, stb, qrcodegen, the
Inter font, Mozilla's CA bundle); `THIRD_PARTY_NOTICES.md` lists each one with
its version and license. Based on
[PSBox Cloud Gaming](https://github.com/RafaelNGP/psbox-cloud-gaming) by
RafaelNGP.

This is an unofficial project, not affiliated with, endorsed or sponsored by
Microsoft or Sony. Xbox, Xbox Cloud Gaming and Game Pass are trademarks of the
Microsoft group of companies; PlayStation is a trademark of Sony Interactive
Entertainment. Game names and artwork are loaded from Microsoft's public
catalog at runtime and are not distributed with this project. Microsoft may
change or block the services this client relies on at any time.
