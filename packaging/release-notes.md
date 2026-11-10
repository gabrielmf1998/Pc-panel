## Tray icon

PC Panel now has a small monitor icon in the system tray.

- Left click opens the dashboard.
- **Change port** asks for a new port and restarts the dashboard on it.
- **Start with system** starts PC Panel when you log in, or not.
- **Quit** stops the dashboard service and closes the icon. Nothing keeps running.

The icon is written in C on sd-bus (part of systemd), with no GUI toolkit, no timers and no polling.
Measured: about 0.3 MiB of private memory and 0 CPU while idle.

## Updates from inside the program

Settings, "Check for a new version", then "Update now". It works for every install type:
the quick install and the AppImage update themselves; the Arch and Fedora packages are installed
through the package manager after you type the administrator password.
Every download is checked against `SHA256SUMS` before it is used.

## Files

| File | For |
|---|---|
| `pc-panel-*-x86_64.pkg.tar.zst` | Arch Linux: `sudo pacman -U ./pc-panel-*.pkg.tar.zst` |
| `pc-panel-*.x86_64.rpm` | Fedora: `sudo dnf install ./pc-panel-*.rpm` |
| `pc-panel-*-x86_64.AppImage` | Any distribution with systemd and Python 3.12+ |
| `pc-panel-*-x86_64.tar.gz` | Used by the curl installer and the self-updater |
| `pc-panel-*.tar.gz` | Source |

Quick install for one user, no root:

```sh
curl -fsSL https://raw.githubusercontent.com/gabrielmf1998/pc-panel/main/install.sh | sh
```
