## 0.5.2: notebooks, integrated graphics, battery

- **Intel integrated graphics** (i915 and xe drivers): usage and video engine use from the programs'
  DRM statistics, clock, and video memory per program. The card is called "Video memory" and counts
  GiB taken from RAM, with no alert limit of its own (the RAM alert covers it).
- **AMD integrated graphics** (APUs) are told apart from graphics cards. Their memory is the small
  reserved part plus the shared part from RAM, so the card no longer shows only 0.5 GiB. Programs
  using the video memory are listed for AMD too.
- **Battery card** (module on by default, hidden on PCs without a battery): charge, time left or time
  to full (from the average of the last 2 minutes, and up to the charge limit when one is set), power
  drawn or charging, health against the design capacity, cycles, chemistry, age from the manufacture
  date, charge limits, voltage and temperature when the kernel gives them. A low battery alert while
  unplugged, and battery % and watts in the 24 h history and in the CSV log. Headset and mouse
  batteries stay in the System card.
- **Notebook fans** from other fan chips (ThinkPad, Dell, ASUS and others) are shown read only in
  Temperatures, and the fan control buttons hide when there is nothing to control.
- **Small screens**: checked at 1366x768 and 1280x720; empty fields hide instead of leaving gaps.
- Processor names are shorter for Intel mobile chips (i5-1235U, Ultra 7 155H, Celeron N4020).
- Tested on Fedora 45 (Python 3.15): the RPM builds, installs and runs.

## 0.5.1: alerts that stand out, disks, bottleneck

- **Alerts stand out again.** Metric colors were moved away from the alert colors (no more pink or
  green lines that look like a status). A card in alert gets a filled badge (HIGH, CRITICAL), its
  chart line turns the level color, the part of the chart above the alert limit is shaded red, and a
  critical card pulses slowly (it stays still when the system asks for reduced motion). The processor and
  graphics card cards also follow their temperature level. VRAM, processor and ping charts show
  their alert limit, and a limit just above the data is pulled into view.
- **Disks on two lines**: name and free space, then health, years left, temperature, read and write
  speed and busy share. The model moved to the tooltip, and the activity line no longer disappears.
- **Game bottleneck.** The Game card says whether the game is limited by the graphics card (above
  95 % use), by the processor (a game thread above 85 % of one core) or by an FPS cap (very steady
  frames with neither busy). The game's threads are read every 10 s.
- **Frame time chart** next to the FPS chart: the worst frame of each reading, where stutters show.
- **Temperatures**: the bar now fills up to the first alert limit, and the highest reading of the last
  24 h is shown next to each sensor (kept across restarts).
- **System card**: two lines with power, internet used today (download and upload, kept across
  restarts), battery, kernel and driver.
- **Events strip** is taller and draws the processor temperature of each minute over the 24 h.
- Measured with VRChat open and the page closed: 0.09 % of one core.

## 0.5.0: game FPS, livelier charts, 24 h strip

- **Game FPS from MangoHud.** When MangoHud logging is on (see the README), the Game card shows FPS,
  frame time and the worst frame of the last reading, the session average and 1 % low, and a 10 minute
  FPS chart where the tooltip gives the worst frame of each point. The panel reads only the new lines
  of the log the game is writing, and deletes its old logs when they sit in a temporary folder. FPS
  also goes into the 24 h history (average and lowest per minute) and into game sessions.
- **Charts zoom to the data.** RAM, VRAM, graphics card and processor charts fit their range to what
  happened in the last 10 minutes instead of 0 to 100 %, so small changes are visible. Each metric
  has its own color (with a gradient under the line) and a matching mark next to the card title;
  a limit line outside the range turns into a small note.
- **Numbers change color by level**: the big number on RAM, VRAM, graphics card, processor and
  network turns yellow, orange or red with the alert level.
- **Idle modules become chips.** With the OBS or Project Zomboid module on but OBS closed or the
  server off, the card is replaced by a small chip in the top bar, leaving room for the rest.
- **Events card with the last 24 h**: a strip with the time a game was open and the minutes with an
  alert in the level color, plus one line with time gaming and the peak temperatures. Fetched every
  5 minutes, only while the card is visible.
- **Power estimate.** The System line shows the graphics card power, and the processor plus graphics
  card total when the processor energy counter (RAPL) is readable by your user.
- **Narrow windows**: the compact Game card shows "game · FPS" in one line, chips hide below 700 px,
  and chart axis labels drop the ones that would overlap.

## 0.4.3: packages outside the repositories

- **The update check now says what it cannot check.** On Arch, packages from outside the official
  repositories (AUR, built by hand) are listed in System health, "Outside the repositories", with
  their names in the tooltip and in Settings. The panel never contacts the AUR; it only reads the
  local package list (`pacman -Qm`) when you click check updates.

## 0.4.2: network view, SSD forecast, incident report

- **Network view.** The Network card says in one line whether the connection is good and where the
  trouble is; its details button opens a full screen view. The router is pinged next to the internet
  target, so the view tells "between this PC and the router" (cable, Wi-Fi, the router) from "past
  the router" (the provider), and says when the PC itself dropped UDP packets. Ping chart for the
  internet and the router (10 minutes, lost pings marked), traffic chart, a quality table (now,
  median, worst 5 %, jitter, loss), programs on the network (TCP download and upload per program,
  TCP and UDP socket counts) and the connection details (link, duplex, MTU, Wi-Fi signal, card
  errors, TCP resent, UDP dropped, totals since boot).
- **Costs nothing while closed.** The per-program list is read only while the view is open (every
  5 s, 10 s while gaming). Measured over 120 s each, against 0.4.1 on the same PC: page closed 0.108 %
  of one core in both; page open 0.592 % in both; network view open 0.675 % (0.08 % more). Memory
  about 1.5 MiB more.
- **Jitter alert** (recommended 20/40/80 ms). The 24 h history now keeps worst packet loss, jitter,
  download and upload, and game sessions keep average ping, worst loss and jitter.
- **SSD life forecast** for NVMe drives: terabytes per 1 % of wear, terabytes left to 100 % of the
  rated wear and, after a day of measured writes, the years left at your pace. Marked rough while
  the life used is under 5 %.
- **Incident report.** Each unexpected stop in the black box comes with a short list of what changed
  in the final 2 minutes (temperatures, processor clock, free RAM, swap, disk waits, lost pings).
- **Desktop notifications are off by default** on new installs; the tray icon and the page already
  show every alert. A choice already saved in Settings is kept.

## 0.4.1: black box writes 27 times less

- **The black box wrote far more than it should.** Each 512 byte record plus sync went through the
  page cache, and on recent kernels that dirties a whole large folio: measured about 1.3 MB a minute
  on ext4 instead of a few KiB. Records now go straight to the disk (O_DIRECT, one 4 KiB block):
  measured 48 KiB a minute with the page open. File systems without O_DIRECT keep the old way.

## 0.4.0: black box, 24 h history, game sessions, SSD health

- **Black box.** A summary of every sample goes to a small file of fixed size (180 KiB, the last 30
  to 60 minutes) that is written in place and synced, so it survives a freeze or a power cut. When
  the PC stops without a normal shutdown, the next boot raises an alert with the time it stopped and
  History, Black box shows the last minutes (temperatures, clocks, power, VRAM, free RAM, swap, RAM
  pressure, disk wait, ping, top program, game).
- **24 h history.** One row per minute, saved every 15 minutes, as a chart per metric (processor,
  graphics card, temperatures, power, VRAM, free RAM, swap, ping) with the minutes a game ran marked.
- **Game sessions.** When a game closes its duration, average use, peak temperatures and power, VRAM
  peak, lowest free RAM, game RAM and alerts are saved; the Game card shows the running session.
- **SSD health** through SMART (UDisks2, no root): NVMe life used, spare, data written, hours, unsafe
  shutdowns and media errors; SATA SMART status, spare, bad sectors. New alerts for life used, a
  health problem reported by the drive, and any new read/write error.
- **Gaming thresholds.** A second set of thresholds applies while a game runs (VRAM 95/98 %,
  processor 95/99 % recommended), edited in the same table with a Normal / Gaming switch.
- **Silence alerts** for 15 min, 1 h or 3 h, one alert or all, from the banner or the Events window.
- **Tray tooltip readings** (processor, graphics card, VRAM, free RAM, ping, game time), at most
  every 30 s and only when they changed. Optional.
- **Full screen shows everything**: on a 4 column window rows get shorter before any card is hidden;
  smaller windows still keep the most important cards. List rows that do not fit whole are hidden
  instead of cut in half.
- **Never competes with a game.** The service now runs with the SCHED_IDLE processor class and the
  idle disk class (only idle time), and with a game running the open page refreshes every 5 s at
  most and the program list every 10 s.
- **Measured cost** (120 s runs, share of one processor core): page closed 0.075 % before and after,
  memory 66 to 68 MiB; page open 0.45 % before, 0.30 % now (partly run to run noise); page open with
  a game running 0.16 %. The black box writes one record every 5 to 15 s (see 0.4.1 for its real disk cost).

## 0.3.3: fix

- **Programs card kept losing the "CPU by program" column** when the window got narrower. Now the
  three lists stay side by side and only the bars are left out to make room; on a very narrow card
  the lists stack.

## 0.3.2: fixes

- **Tray icon could turn into a blank page on an alert.** Plasma did not load the colored icon by
  its name and showed its "unknown" icon instead. The tray now sends the four icons as small images
  built into the program (read only memory, 16 to 48 px), the same way Electron apps do, so it no
  longer depends on an icon lookup.
- **Alert colors follow the theme.** Each theme has its own yellow, orange and red, all readable:
  darker on the light theme (the old yellow was hard to read there), brighter on high contrast.
- **Headset or mouse battery** is now in the thresholds table (recommended 15, 8 and 3 %), instead of
  a fixed 15 % warning.

## 0.3.1: lighter

The dashboard now sleeps until its next task is due, and the page opening wakes it at once,
instead of checking twice a second. Measured on the same PC over 2 minutes with the page closed:

| | 0.3.0 | 0.3.1 |
|---|---|---|
| Background watch on (default, every 10 s) | 7.5 wakeups/s, 0.10 % of one core | 1.9 wakeups/s, 0.08 % of one core |
| Background watch off | 6.7 wakeups/s | 0.35 wakeups/s, 0.00 % |

Memory is unchanged (about 34 MiB, or about 60 MiB once the NVIDIA library is loaded). Opening the
page now also refreshes the program list and fans at once instead of up to 30 s later.

## Alerts you can see

The tray icon now changes color when something needs attention: **yellow** for a warning,
**orange** for serious, **red** for critical. Its tooltip lists the problems. The icon stays static,
with no animation, and still uses no timers: the dashboard writes one tiny file and the icon is
woken by inotify only when the level changes.

Watched: low free RAM, VRAM use, processor use (30 s average), a single program using the processor,
processor, graphics card, SSD, memory and motherboard temperatures, disk space, ping, packet loss,
kernel errors and crashed programs.

## Configurable thresholds with a hardware based "Recommended"

Settings, Alerts has a table with warning, serious and critical values for every alert. Each row has
**Recommended**, which reads the limits of your own hardware:

- Graphics card: NVIDIA max operating and slowdown temperatures from the driver, AMD critical from hwmon
- Processor: AMD Tjmax per model, Intel coretemp critical
- SSDs: every NVMe drive's own warning and critical temperatures, per drive
- Memory: the DDR5 SPD sensor critical temperature
- Free RAM: scaled to the installed RAM

Untick Recommended to type your own values; an empty box turns that level off. "Reset everything to
recommended" undoes all custom values. Kernel errors and crashes can be turned off or given a level.

## Also

- With the page closed the dashboard keeps a light watch (every 10 s by default, configurable or off),
  so the tray icon and desktop notifications work without the page open.
- Desktop notifications can start from warning, serious or critical, and a problem that gets worse
  now notifies when it crosses that level.

## Files

| File | For |
|---|---|
| `pc-panel-*-x86_64.pkg.tar.zst` | Arch Linux: `sudo pacman -U ./pc-panel-*.pkg.tar.zst` |
| `pc-panel-*.x86_64.rpm` | Fedora: `sudo dnf install ./pc-panel-*.rpm` |
| `pc-panel-*-x86_64.AppImage` | Any distribution with systemd and Python 3.12+ |
| `pc-panel-*-x86_64.tar.gz` | Used by the curl installer and the self-updater |
| `pc-panel-*.tar.gz` | Source |

Already installed? Settings, "Check for a new version", then "Update now".

Quick install for one user, no root:

```sh
curl -fsSL https://raw.githubusercontent.com/gabrielmf1998/pc-panel/main/install.sh | sh
```
