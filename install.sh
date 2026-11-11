#!/bin/sh
# PC Panel installer for one user, no root needed for the dashboard and the tray icon.
#   curl -fsSL https://raw.githubusercontent.com/gabrielmf1998/pc-panel/main/install.sh | sh
# Options:  --fans       also enable fan control (asks for the administrator password)
#           --uninstall  remove everything this script installed
#           --update     used by the self-updater: install over the current copy, keep every choice
set -eu
REPO="gabrielmf1998/pc-panel"
PREFIX="$HOME/.local"
CONF="${XDG_CONFIG_HOME:-$HOME/.config}"
UNIT="$CONF/systemd/user/pc-panel.service"
AUTOSTART="$CONF/autostart/pc-panel.desktop"
ROOT_LIB=/usr/local/lib/pc-panel
FANS=no
ACTION=install
for a in "$@"; do
    case "$a" in
        --fans) FANS=yes ;;
        --uninstall) ACTION=uninstall ;;
        --update) ACTION=update ;;
        *) echo "unknown option: $a" >&2; exit 2 ;;
    esac
done
root() { if [ "$(id -u)" = 0 ]; then "$@"; elif command -v sudo >/dev/null; then sudo "$@"; else pkexec "$@"; fi; }
tray_running() { pgrep -x -u "$(id -u)" pc-panel-tray >/dev/null 2>&1; }

if [ "$ACTION" = uninstall ]; then
    pkill -x -u "$(id -u)" pc-panel-tray 2>/dev/null || true
    systemctl --user disable --now pc-panel.service 2>/dev/null || true
    rm -f "$PREFIX/bin/pc-panel" "$PREFIX/bin/pc-panel-tray" "$UNIT" "$AUTOSTART" \
          "$PREFIX/share/applications/pc-panel.desktop" "$PREFIX"/share/icons/hicolor/scalable/apps/pc-panel*.svg
    rm -rf "$PREFIX/share/pc-panel" "$PREFIX/lib/pc-panel"
    systemctl --user daemon-reload 2>/dev/null || true
    if [ -e "$ROOT_LIB/pc-panel-fans" ]; then
        echo "Removing the fan helper (administrator password)"
        root sh -c "systemctl disable --now pc-panel-fans.path pc-panel-fans.service 2>/dev/null; rm -rf $ROOT_LIB /etc/systemd/system/pc-panel-fans.path /etc/systemd/system/pc-panel-fans.service /var/lib/pc-panel; systemctl daemon-reload"
    fi
    echo "PC Panel removed. Settings were kept in $CONF/pc-panel"
    exit 0
fi

command -v python3 >/dev/null || { echo "python3 is required" >&2; exit 1; }
python3 -c 'import sys; sys.exit(0 if sys.version_info >= (3, 12) else 1)' || { echo "Python 3.12 or newer is required" >&2; exit 1; }
command -v systemctl >/dev/null || { echo "systemd is required" >&2; exit 1; }

SRC="$(cd "$(dirname "$0")" 2>/dev/null && pwd || true)"
TMP=""
if [ ! -f "$SRC/bin/pc-panel" ]; then
    # running from curl: fetch the newest release (with the compiled tray icon for this
    # machine when there is one), or the main branch if nothing is published yet
    TMP="$(mktemp -d)"
    trap 'rm -rf "$TMP"' EXIT
    TAG="$(curl -fsSL "https://api.github.com/repos/$REPO/releases/latest" 2>/dev/null | sed -n 's/.*"tag_name": *"\([^"]*\)".*/\1/p' | head -n 1)"
    if [ -n "$TAG" ]; then
        BASE="https://github.com/$REPO/releases/download/$TAG"
        V="${TAG#v}"
        NAME="pc-panel-$V-$(uname -m).tar.gz"
        curl -fsSL -o "$TMP/$NAME" "$BASE/$NAME" 2>/dev/null || { NAME="pc-panel-$V.tar.gz"; curl -fsSL -o "$TMP/$NAME" "$BASE/$NAME"; }
        curl -fsSL -o "$TMP/SHA256SUMS" "$BASE/SHA256SUMS"
        (cd "$TMP" && grep " \*\{0,1\}$NAME\$" SHA256SUMS | sha256sum -c - >/dev/null) || { echo "Checksum mismatch for $NAME" >&2; exit 1; }
        echo "Downloaded PC Panel $V ($NAME)"
    else
        NAME=main.tar.gz
        echo "No release published yet, using the main branch"
        curl -fsSL -o "$TMP/$NAME" "https://github.com/$REPO/archive/refs/heads/main.tar.gz"
    fi
    mkdir "$TMP/src"
    tar -xzf "$TMP/$NAME" -C "$TMP/src" --strip-components=1
    SRC="$TMP/src"
fi

# the tray icon: use the compiled one when it runs here, otherwise compile it
TRAY=""
if [ -x "$SRC/bin/pc-panel-tray" ] && "$SRC/bin/pc-panel-tray" --version >/dev/null 2>&1; then
    TRAY="$SRC/bin/pc-panel-tray"
elif command -v cc >/dev/null && [ -f /usr/include/systemd/sd-bus.h ]; then
    OUT="$(mktemp)"
    V="$(sed -n 's/^VERSAO = "\(.*\)"/\1/p' "$SRC/bin/pc-panel")"
    if cc -O2 -DVERSION="\"$V\"" -o "$OUT" "$SRC/tray/pc-panel-tray.c" -lsystemd 2>/dev/null; then
        TRAY="$OUT"
    fi
fi
[ -n "$TRAY" ] || echo "Note: no tray icon for this machine (it needs a C compiler and the systemd headers). The dashboard still works."

FRESH=no
[ -e "$PREFIX/bin/pc-panel" ] || FRESH=yes

# put each file in place with a rename, so a running tray never sees half a file
put() { mkdir -p "$(dirname "$3")"; install -m "$1" "$2" "$3.new"; mv -f "$3.new" "$3"; }
put 644 "$SRC/share/pc-panel/page.html" "$PREFIX/share/pc-panel/page.html"
put 755 "$SRC/lib/pc-panel/pc-panel-fans" "$PREFIX/lib/pc-panel/pc-panel-fans"
for i in "$SRC"/share/icons/hicolor/scalable/apps/pc-panel*.svg; do
    put 644 "$i" "$PREFIX/share/icons/hicolor/scalable/apps/${i##*/}"
done
put 755 "$SRC/bin/pc-panel" "$PREFIX/bin/pc-panel"
mkdir -p "$(dirname "$UNIT")"
sed "s|@BIN@|$PREFIX/bin/pc-panel|" "$SRC/systemd/pc-panel.service" > "$UNIT.new" && mv -f "$UNIT.new" "$UNIT"
if [ -n "$TRAY" ]; then
    put 755 "$TRAY" "$PREFIX/bin/pc-panel-tray"
    mkdir -p "$PREFIX/share/applications"
    sed "s|@TRAY@|$PREFIX/bin/pc-panel-tray|" "$SRC/share/applications/pc-panel.desktop" > "$PREFIX/share/applications/pc-panel.desktop"
fi
[ -n "${OUT:-}" ] && rm -f "$OUT"
systemctl --user daemon-reload
systemctl --user reset-failed pc-panel.service 2>/dev/null || true

if [ "$FRESH" = yes ] && [ "$ACTION" = install ]; then
    # a new install starts with the system; the tray icon turns this off and on
    systemctl --user enable pc-panel.service >/dev/null 2>&1
    if [ -n "$TRAY" ]; then
        mkdir -p "$(dirname "$AUTOSTART")"
        printf '[Desktop Entry]\nType=Application\nName=PC Panel\nComment=Tray icon of the PC Panel dashboard\nExec="%s"\nIcon=pc-panel\nTerminal=false\nNoDisplay=true\nX-GNOME-Autostart-enabled=true\n' \
            "$PREFIX/bin/pc-panel-tray" > "$AUTOSTART"
    fi
fi

if tray_running; then
    :   # the running tray icon sees the new files, restarts the dashboard and itself
elif [ "$ACTION" = update ]; then
    systemctl --user try-restart pc-panel.service
else
    systemctl --user restart pc-panel.service
    if [ -n "$TRAY" ] && { [ -n "${WAYLAND_DISPLAY:-}" ] || [ -n "${DISPLAY:-}" ]; }; then
        setsid -f "$PREFIX/bin/pc-panel-tray" >/dev/null 2>&1 < /dev/null || true
    fi
fi

if [ "$FANS" = yes ]; then
    echo "Enabling fan control (administrator password)"
    root sh -c "install -Dm755 '$SRC/lib/pc-panel/pc-panel-fans' $ROOT_LIB/pc-panel-fans \
        && sed 's|@LIB@|$ROOT_LIB|' '$SRC/systemd/pc-panel-fans.service' > /etc/systemd/system/pc-panel-fans.service \
        && install -m644 '$SRC/systemd/pc-panel-fans.path' /etc/systemd/system/pc-panel-fans.path \
        && $ROOT_LIB/pc-panel-fans --allow '$(id -un)' \
        && systemctl daemon-reload && systemctl enable --now pc-panel-fans.path && systemctl enable pc-panel-fans.service"
fi

# the self-updater unpacks into ~/.cache/pc-panel/update: clean it up
case "$SRC" in "${XDG_CACHE_HOME:-$HOME/.cache}/pc-panel/update/"*) rm -rf "${XDG_CACHE_HOME:-$HOME/.cache}/pc-panel/update" ;; esac

[ "$ACTION" = update ] && exit 0
PORT="$(cat "$CONF/pc-panel/port" 2>/dev/null || echo 47822)"
echo
echo "PC Panel $("$PREFIX/bin/pc-panel" --version | cut -d' ' -f2) is running. Open http://127.0.0.1:$PORT"
[ -n "$TRAY" ] && echo "The monitor icon in the tray changes the port, turns start with system on or off, and quits."
[ "$FANS" = yes ] || [ -e "$ROOT_LIB/pc-panel-fans" ] || [ -e /usr/lib/pc-panel/pc-panel-fans ] && [ -e /etc/systemd/system/pc-panel-fans.path -o -e /usr/lib/systemd/system/pc-panel-fans.path ] \
    || echo "Fan control is off. To enable it later run this installer again with --fans"
