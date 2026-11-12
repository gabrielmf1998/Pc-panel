# PC Panel

A local web dashboard to **monitor and control a Linux PC**, made to live on a second screen.
It runs as one small user service and opens at `http://127.0.0.1:47822`. With the page closed it only
takes a light sample every 10 seconds to watch for problems. A tiny tray icon starts it, stops it,
changes its port and turns yellow, orange or red when something needs attention.

The interface is available in English and Portuguese.

## What it shows

- RAM, swap, cache and memory pressure
- Graphics card usage, VRAM, temperature, power, fan speed (NVIDIA, basic AMD)
- Processor usage per core, temperature, clock, load
- Network ping, loss, jitter, download and upload, link speed and errors, and a full network view
- Programs ranked by RAM, VRAM and CPU
- Temperatures and fans, disks with free space, wait time and busy share
- System health: kernel errors, crashed programs, pending reboot, pending updates (pacman or dnf; packages from outside the official repositories, such as the AUR, are counted and listed but never checked)
- The running Steam game with FPS and frame time (through MangoHud logs, see below), an events list
  with a 24 h strip, optional CSV logging
- An estimate of power drawn by the graphics card, and by the processor when its energy counter is readable
- Optional modules, off by default: OBS live card and Project Zomboid server card

## What it controls

Fan control on supported hardware: each motherboard fan can be maximum, a fixed percentage, or a
four point temperature curve stored in the fan chip itself. NVIDIA graphics card fans can be maximum,
automatic, fixed, or follow a custom curve.

Fan control needs one step with the administrator password and is **off until you enable it**.

## Game FPS (MangoHud)

The panel does not hook into games. It reads the log file MangoHud writes, so FPS shows only for games
started with MangoHud and with logging on. Add to `~/.config/MangoHud/MangoHud.conf`:

```
autostart_log=1
log_interval=100
output_folder=/tmp/mangohud-log
```

`autostart_log=1` starts the log one second after the game opens, `log_interval=100` writes a line
every 100 ms (about 1 KiB a second), and a folder under `/tmp` keeps the log in RAM. While logging,
MangoHud draws a small dot at the top right of its overlay; that is normal. The panel reads only the
new lines once per sample and deletes logs older than an hour from a folder under `/tmp` or
`$XDG_RUNTIME_DIR`; it never deletes anything from other folders. Remove the three lines to stop.

## Network view

The Network card says in one line whether the connection is good and, when it is not, where the
trouble is. Its **details** button (or Network in the top bar, on wide windows) opens a full screen
view:

- **Where the trouble is**: the router is pinged next to the internet target, so the view can tell
  "between this PC and the router" (cable, Wi-Fi, the router itself) from "past the router" (the
  provider), and says when the PC itself dropped UDP packets because a program did not read them
  in time.
- **Ping chart** for the internet and the router over the last 10 minutes, lost pings marked, and a
  traffic chart (download and upload).
- **Connection quality**: now, median, worst 5 %, jitter and loss for the internet and the router.
- **Programs on the network**: download and upload per program (TCP), with TCP and UDP socket counts.
  UDP traffic (most online games, voice calls, browser QUIC) has no per-program counter without
  root, so only the number of UDP sockets shows.
- **Connection**: interface, link speed and duplex, MTU, Wi-Fi signal, network card errors, TCP
  resent segments, UDP dropped by the PC, connections, totals since boot.

The per-program list costs something only while this view is open: it reads the socket table every
5 s (10 s while gaming) and stops when the view closes. The router is pinged with the same tiny
packet and at the same pace as the internet target. The card itself stays small (ping, loss, jitter,
rates and the one line verdict), so a small window next to a game shows only what matters.

## Alerts

When something crosses a threshold you see it: the tray icon turns **yellow** (warning), **orange**
(serious) or **red** (critical) and lists the problems in its tooltip, the page shows a banner, and
optionally a desktop notification appears and the page plays a sound. Nothing blinks. Desktop
notifications are **off by default** (the tray icon and the page already show everything); turn them
on in Settings, Alerts.

Watched: low free RAM, VRAM use, processor use (30 s average), one program using the processor,
processor, graphics card, SSD, memory and motherboard temperatures, disk space, ping, packet loss,
ping jitter, kernel errors and crashed programs.

Every threshold is configurable in Settings, Alerts, with three levels each, and every one has a
**Recommended** option that follows your hardware:

| Alert | Recommended comes from |
|---|---|
| Graphics card temperature | NVIDIA driver (NVML): max operating and slowdown temperatures; AMD: hwmon critical |
| Processor temperature | AMD Tjmax per model (89 to 95 °C), Intel coretemp critical |
| SSD temperature | each NVMe drive's own warning and critical temperatures, per drive |
| Memory temperature | the DDR5 SPD sensor's critical temperature |
| Low free RAM | scaled to the installed RAM |
| Everything else | safe fixed values |

The settings page shows where each recommended value came from. An alert starts after two samples
in a row, so a one second spike does not fire it.

**While gaming** a full graphics card and a busy processor are normal, so a second set of thresholds
applies while a game runs (recommended: VRAM 95/98 %, processor 95/99 %, one program 90/98 %; the
rest the same). Both sets are edited in the same table with a Normal / Gaming switch.

**Silence**: the "silence 1 h" chip next to the alert banner mutes everything for an hour, and the
Events window can mute one alert or everything for 15 min, 1 h or 3 h. A muted alert stops coloring
the tray icon and sending notifications; the page still colors the reading itself.

**SSD health** comes from SMART through UDisks2 (no root, read every 30 minutes): NVMe life used,
spare area, data written, power on hours, unsafe shutdowns and media errors; SATA SMART status,
spare area, bad sectors and uncorrectable errors. Alerts: life used (recommended 80/90/97 %), a
health problem reported by the drive, and any new read/write error.

For NVMe drives the panel also gives a **life forecast**: data written divided by the life used gives
the terabytes per 1 % of wear, so it says how many terabytes are left to 100 % of the rated wear and,
once it has seen at least a day of writes (one point every 6 hours, up to 30 days), how many years
that is at your pace. The forecast is marked rough while the life used is under 5 %, because drives
report it as a whole number.

## Black box, 24 h history and game sessions

- **Black box**: a summary of every sample (every 5 s with the page open, 10 s with it closed) goes
  to a file of fixed size (180 KiB, 30 to 60 minutes) that is written in place and synced, so it
  survives a freeze or a power cut. A clean stop leaves a marker; when the next boot finds records
  without it, the PC stopped unexpectedly, the last minutes are kept (up to 10 incidents) and an
  alert says when it stopped. See History, Black box. Each incident comes with a short report of
  what changed in the final 2 minutes against the minutes before them (temperatures that rose, a
  processor clock that fell, free RAM that ran out, swap, disk waits, lost pings), or says nothing
  was out of the ordinary.
- **24 h history**: one row per minute (averages and peaks), saved every 15 minutes, shown as a chart
  per metric with the minutes a game was running marked.
- **Game sessions**: when a game closes (after at least 2 minutes) its duration, average use, peak
  temperatures and power, VRAM peak, lowest free RAM, game RAM, average FPS, 1 % low and alert count
  are saved. The Game card shows the running session.

The black box and the history keep the service sampling every 10 s with the page closed (the same
pace as the background alerts). Both can be turned off in Settings, Measuring.

## Cost while gaming

The service runs with the **SCHED_IDLE** processor class and the **idle** disk class: it only gets
processor time and disk time nobody else wants, so a game is never delayed by it. With a game
running the open page refreshes every 5 s at most (option), and the program list every 10 s.
Each black box record is one 4 KiB block written straight to the disk (O_DIRECT), about 48 KiB a
minute with the page open and half that with it closed.

## Tray icon

A monitor icon in the system tray. Left click opens the dashboard. Its menu has only:

- **Change port**: asks for a new port (1024 to 65535) and restarts the dashboard on it
- **Start with system**: on or off
- **Quit**: stops the dashboard service and closes the icon, so nothing keeps running

Everything else lives in the web page. The icon is a small C program on sd-bus (part of systemd)
with no GUI toolkit, no timers and no polling: about 0.3 MiB of private memory and 0 CPU while idle.
It needs a tray that speaks StatusNotifierItem (KDE Plasma, XFCE, LXQt, Cinnamon, and GNOME with the
AppIndicator extension). The port dialog uses `kdialog` or `zenity`.

Hovering the icon shows a few readings (processor, graphics card, VRAM, free RAM, ping, game time),
rewritten at most every 30 seconds and only when they changed. It can be turned off in Settings.

## Install

Quick, for one user, no root needed:

```sh
curl -fsSL https://raw.githubusercontent.com/gabrielmf1998/pc-panel/main/install.sh | sh
```

Add fan control (asks for the administrator password):

```sh
curl -fsSL https://raw.githubusercontent.com/gabrielmf1998/pc-panel/main/install.sh | sh -s -- --fans
```

Other ways, from the [releases page](https://github.com/gabrielmf1998/pc-panel/releases)
(also mirrored on [GitLab](https://gitlab.com/gabriel17166/pc-panel/-/releases)):

| System | How |
|---|---|
| Arch Linux | `sudo pacman -U ./pc-panel-*-x86_64.pkg.tar.zst`, or build `packaging/arch/PKGBUILD` with `makepkg -si` |
| Fedora | `sudo dnf install ./pc-panel-*.x86_64.rpm` |
| Any distro | `chmod +x pc-panel-*-x86_64.AppImage` and run it (uses the system python3) |

After a package install, open **PC Panel** from the application menu once and tick **Start with system** in the tray menu.

Remove a quick install with `curl -fsSL https://raw.githubusercontent.com/gabrielmf1998/pc-panel/main/install.sh | sh -s -- --uninstall`.

## Requirements and supported hardware

- Linux with systemd and Python 3.12 or newer. No Python packages needed.
- The tray icon is built for x86_64. On other machines the quick installer compiles it when a C compiler and the systemd headers exist.
- Graphics card readings: NVIDIA with the proprietary driver, or AMD through the kernel driver. Without one, those cards are hidden.
- Fan control and motherboard fan speeds: Nuvoton NCT67xx fan chips (driver `nct6775`). Other chips are not supported yet.
- Developed and tested on one Arch Linux machine (Ryzen, RTX 3060 Ti). Fedora packages are built and installed by the release workflow but have not been used on real hardware yet. Reports are welcome.

## Updating

Settings has a "Check for a new version" button, then "Update now". The download is checked against the
published `SHA256SUMS` first. The quick install and the AppImage replace their own files; the Arch and Fedora
packages are installed with `pacman -U` or `dnf install` after you type the administrator password.
The tray icon notices the new files and reloads itself and the dashboard.

## Safety notes

- The service listens only on `127.0.0.1`, rejects requests with a foreign `Host` or `Origin`, and needs a custom header for any change.
- It only reads the system. It writes its own settings, the optional CSV log and the fan choice file.
- The root fan helper runs for a moment when the choice changes or the PC boots. It validates everything it reads and falls back to 100 % on anything unexpected.
- Cost measured on the development machine: about 0.6 % of one core and 40 MiB with the page open, nothing when idle.

## Layout of this repository

```
bin/pc-panel                 the service (Python standard library only)
tray/pc-panel-tray.c         the tray icon (C, sd-bus)
share/pc-panel/page.html     the page
lib/pc-panel/pc-panel-fans   the root fan helper
systemd/                     unit templates
install.sh                   quick installer
packaging/                   Arch, RPM and AppImage recipes
```

Identifiers inside the code are still in Portuguese and will be renamed to English in a later release. The interface texts live in one table at the top of the page file.

## License

MIT
