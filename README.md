# PC Panel

A local web dashboard to **monitor and control a Linux PC**, made to live on a second screen.
It runs as one small user service and opens at `http://127.0.0.1:47822`. With the page closed it only
takes a light sample every 10 seconds to watch for problems. A tiny tray icon starts it, stops it,
changes its port and turns yellow, orange or red when something needs attention.

The interface is available in English and Portuguese.

## What it shows

- RAM, swap, cache and memory pressure
- Graphics card usage, VRAM, temperature, power, fan speed, memory clock, PCIe link and why the card holds its clock back under load (heat, hardware or power limit) (NVIDIA, basic AMD); Intel and AMD integrated graphics with usage, clock and video memory per program
- Processor usage with a strip of every core, temperature, clock, load
- Network ping, loss, jitter, download and upload, link speed and errors, and a full network view
- Programs ranked by RAM, VRAM and CPU; scripts are named after the script (a Python or Node program shows its own name), and a click on a program lists its processes with RAM, CPU, start time, service and command line
- Notebook battery: charge, time left or to full, power, health, cycles, age and charge limits (module, shown only when a battery exists)
- Temperatures and fans (with the highest reading of the last 24 h, notebook fans read only), disks with health, temperature, speed and busy share
- System health: kernel errors, crashed programs, pending reboot, pending updates (pacman or dnf; packages from outside the official repositories, such as the AUR, are counted and listed but never checked)
- The running Steam game with FPS, frame time and what limits it (graphics card, processor or an FPS cap), an events list
  with a 24 h strip (a click on an event opens the 24 h history at that moment), optional CSV logging
- Charts show the last 10 minutes, and a click on the period label switches them to the last hour or the last 24 h
- Power: processor package and graphics card in watts, what is normal on this PC (idle, normal, gaming, high), peaks with what was running, energy per day and month with an optional cost, and a power alert based on your power supply (see Power below); internet used today
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

## Power

The Power card shows what the processor and the graphics card draw right now, next to the **normal
value of this PC** (the median of the last 24 h, one value per minute) and the peak of the day. A bar
compares now with the highest peak of the last 24 h, with a mark at the normal value, and the badge
says whether the reading is idle, normal, above normal or near the peak. Its **details** button opens
a full screen view (also at `#energia`):

- **Chart** of total, processor and graphics card, with the normal value as a dashed line. The 1 h
  and 24 h periods show the peak of each minute, so a short spike is not averaged away.
- **What is normal here**, for the total and each part: idle (lowest 10 % of the minutes), normal
  (median), gaming (median of the minutes with a game open), high (95th percentile) and the peak.
- **Biggest peaks of 24 h**, one per 15 minutes, with the processor and graphics card share and the
  game or busiest program at that moment.
- **Energy per day** for the last 30 days (kWh, measured time, peak, energy while gaming), today and
  this month, and the cost when a price per kWh is set in Settings, Power.
- **Limits**: the graphics card power limit in force, its factory default and the range it accepts,
  the share of busy time it sat at its power limit or was braked by the hardware, and how much of the
  power supply the 24 h peak uses when its rating is set in Settings.

Game sessions also keep the average power and the energy used (Wh), and the CSV log has
`cpu_watts` and `total_watts` columns.

Where the numbers come from, no root needed:

- **Processor**: AMD Ryzen processors with integrated graphics (every Ryzen 7000 and 9000, and the
  APUs) report the power of the whole chip through the `amdgpu` sensor. Intel and AMD RAPL counters
  are used when your user can read them (most distributions keep them root only, and the panel does
  not change that).
- **Graphics card**: NVIDIA through the driver library, including its energy counter, so the energy
  between two samples is exact; AMD cards through the kernel sensor.
- On battery, the notebook's whole draw from the battery.

Each reading is about a one second average, so millisecond spikes do not show. The PC pulls more than
this from the wall: the motherboard, memory, disks, fans and the power supply loss have no sensor.

The power alert (processor + graphics card, 10 s average) has no recommended value until the power
supply rating is typed in Settings, Power; then it warns at 60, 75 and 90 % of it.

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
  per metric with the minutes a game was running marked. The page fetches only the new minutes.
  The same rows feed the 1 h and 24 h periods of the main charts.
- **Game sessions**: when a game closes (after at least 2 minutes) its duration, average use, peak
  temperatures and power, VRAM peak, lowest free RAM, game RAM, average FPS, 1 % low and alert count
  are saved, with the share of time the graphics card held its clock for heat or power. The Game
  card shows the running session. Tick two sessions to compare them side by side.

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
curl -fsSL https://raw.githubusercontent.com/gabrielmf1998/Pc-panel/main/install.sh | sh
```

Add fan control (asks for the administrator password):

```sh
curl -fsSL https://raw.githubusercontent.com/gabrielmf1998/Pc-panel/main/install.sh | sh -s -- --fans
```

Other ways, from the [releases page](https://github.com/gabrielmf1998/Pc-panel/releases)
(also mirrored on [GitLab](https://gitlab.com/gabriel17166/pc-panel/-/releases)):

| System | How |
|---|---|
| Arch Linux | `sudo pacman -U ./pc-panel-*-x86_64.pkg.tar.zst`, or build `packaging/arch/PKGBUILD` with `makepkg -si` |
| Fedora | `sudo dnf install ./pc-panel-*.x86_64.rpm` |
| Any distro | `chmod +x pc-panel-*-x86_64.AppImage` and run it (uses the system python3) |

After a package install, open **PC Panel** from the application menu once and tick **Start with system** in the tray menu.

Remove a quick install with `curl -fsSL https://raw.githubusercontent.com/gabrielmf1998/Pc-panel/main/install.sh | sh -s -- --uninstall`.

## Requirements and supported hardware

- Linux with systemd and Python 3.12 or newer. No Python packages needed.
- The tray icon is built for x86_64. On other machines the quick installer compiles it when a C compiler and the systemd headers exist.
- Graphics card readings: NVIDIA with the proprietary driver, AMD through the kernel driver, or Intel integrated graphics (i915 or xe driver). Without one, those cards are hidden. Integrated graphics have no power or temperature reading of their own (on AMD the chip power shows as the processor in the Power card).
- Fan control and motherboard fan speeds: Nuvoton NCT67xx fan chips (driver `nct6775`). Other chips are not supported yet.
- Developed and tested on one Arch Linux machine (Ryzen, RTX 3060 Ti, Radeon integrated graphics). Fedora packages are built by the release workflow and were run in a Fedora 45 container; Intel graphics and notebook batteries were tested with simulated kernel files, not on a real notebook yet. Reports are welcome.

## Updating

Settings has a "Check for a new version" button, then "Update now". The download is checked against the
published `SHA256SUMS` first. The quick install and the AppImage replace their own files; the Arch and Fedora
packages are installed with `pacman -U` or `dnf install` after you type the administrator password.
The tray icon notices the new files and reloads itself and the dashboard.

## Safety notes

- The service listens only on `127.0.0.1`, rejects requests with a foreign `Host` or `Origin`, and needs a custom header for any change.
- It only reads the system. It writes its own settings, the optional CSV log and the fan choice file.
- The root fan helper runs for a moment when the choice changes or the PC boots. It validates everything it reads and falls back to 100 % on anything unexpected.
- Cost measured on the development machine with the page open: about 0.35 % of one core for the service and under 1 % for the browser tab, about 70 MiB of RAM (a third of it the NVIDIA library).

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
