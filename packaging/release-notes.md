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
