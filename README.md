# PC Panel

A local web dashboard to **monitor and control a Linux PC**, made to live on a second screen.
It runs as one small user service, opens at `http://127.0.0.1:47822`, and sleeps when the page is closed.

The interface is available in English and Portuguese.

## What it shows

- RAM, swap, cache and memory pressure
- Graphics card usage, VRAM, temperature, power, fan speed (NVIDIA, basic AMD)
- Processor usage per core, temperature, clock, load
- Network ping, loss, jitter, download and upload, link speed and errors
- Programs ranked by RAM, VRAM and CPU
- Temperatures and fans, disks with free space, wait time and busy share
- System health: kernel errors, crashed programs, pending reboot, pending updates (pacman or dnf)
- The running Steam game, an events list, optional CSV logging
- Optional modules, off by default: OBS live card and Project Zomboid server card

## What it controls

Fan control on supported hardware: each motherboard fan can be maximum, a fixed percentage, or a
four point temperature curve stored in the fan chip itself. NVIDIA graphics card fans can be maximum,
automatic, fixed, or follow a custom curve.

Fan control needs one step with the administrator password and is **off until you enable it**.

## Install

Quick, for one user, no root needed:

```sh
curl -fsSL https://raw.githubusercontent.com/gabrielmf1998/pc-panel/main/install.sh | sh
```

Add fan control (asks for the administrator password):

```sh
curl -fsSL https://raw.githubusercontent.com/gabrielmf1998/pc-panel/main/install.sh | sh -s -- --fans
```

Other ways, from the [releases page](https://github.com/gabrielmf1998/pc-panel/releases):

| System | How |
|---|---|
| Arch Linux | `packaging/arch/PKGBUILD` with `makepkg -si` |
| Fedora | `sudo dnf install ./pc-panel-*.noarch.rpm` |
| Any distro | `pc-panel-*-x86_64.AppImage` (dashboard only, uses the system python3) |

After a package install, start it for your user with `systemctl --user enable --now pc-panel`.

Remove a quick install with `sh install.sh --uninstall`.

## Requirements and supported hardware

- Linux with systemd and Python 3.12 or newer. No Python packages needed.
- Graphics card readings: NVIDIA with the proprietary driver, or AMD through the kernel driver. Without one, those cards are hidden.
- Fan control and motherboard fan speeds: Nuvoton NCT67xx fan chips (driver `nct6775`). Other chips are not supported yet.
- Developed and tested on one Arch Linux machine (Ryzen, RTX 3060 Ti). Fedora packages are built and installed by the release workflow but have not been used on real hardware yet. Reports are welcome.

## Updating

Settings has a "Check for a new version" button. With the quick install or the AppImage it downloads the
new release, checks it against the published SHA-256 and swaps the files. With a distro package it only
tells you a new version exists, because the package manager owns the files.

## Safety notes

- The service listens only on `127.0.0.1`, rejects requests with a foreign `Host` or `Origin`, and needs a custom header for any change.
- It only reads the system. It writes its own settings, the optional CSV log and the fan choice file.
- The root fan helper runs for a moment when the choice changes or the PC boots. It validates everything it reads and falls back to 100 % on anything unexpected.
- Cost measured on the development machine: about 0.6 % of one core and 40 MiB with the page open, nothing when idle.

## Layout of this repository

```
bin/pc-panel                 the service (Python standard library only)
share/pc-panel/page.html   the page
lib/pc-panel/pc-panel-fans   the root fan helper
systemd/                     unit templates
install.sh                   quick installer
packaging/                   Arch, RPM and AppImage recipes
```

Identifiers inside the code are still in Portuguese and will be renamed to English in a later release. The interface texts live in one table at the top of the page file.

## License

MIT
