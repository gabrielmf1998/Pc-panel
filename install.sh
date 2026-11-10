#!/bin/sh
# PC Panel installer for one user, no root needed for the dashboard itself.
#   curl -fsSL https://raw.githubusercontent.com/gabrielmf1998/pc-panel/main/install.sh | sh
# Options:  --fans       also enable fan control (asks for the administrator password)
#           --uninstall  remove everything this script installed
set -eu
REPO="gabrielmf1998/pc-panel"
PREFIX="$HOME/.local"
UNIT="${XDG_CONFIG_HOME:-$HOME/.config}/systemd/user/pc-panel.service"
ROOT_LIB=/usr/local/lib/pc-panel
FANS=no
ACTION=install
for a in "$@"; do
    case "$a" in
        --fans) FANS=yes ;;
        --uninstall) ACTION=uninstall ;;
        *) echo "unknown option: $a" >&2; exit 2 ;;
    esac
done
root() { if [ "$(id -u)" = 0 ]; then "$@"; elif command -v sudo >/dev/null; then sudo "$@"; else pkexec "$@"; fi; }

if [ "$ACTION" = uninstall ]; then
    systemctl --user disable --now pc-panel.service 2>/dev/null || true
    rm -f "$PREFIX/bin/pc-panel" "$UNIT"
    rm -rf "$PREFIX/share/pc-panel" "$PREFIX/lib/pc-panel"
    systemctl --user daemon-reload 2>/dev/null || true
    if [ -e "$ROOT_LIB/pc-panel-fans" ]; then
        echo "Removing the fan helper (administrator password)"
        root sh -c "systemctl disable --now pc-panel-fans.path pc-panel-fans.service 2>/dev/null; rm -rf $ROOT_LIB /etc/systemd/system/pc-panel-fans.path /etc/systemd/system/pc-panel-fans.service /var/lib/pc-panel; systemctl daemon-reload"
    fi
    echo "PC Panel removed. Settings were kept in ${XDG_CONFIG_HOME:-$HOME/.config}/pc-panel"
    exit 0
fi

command -v python3 >/dev/null || { echo "python3 is required" >&2; exit 1; }
python3 -c 'import sys; sys.exit(0 if sys.version_info >= (3, 12) else 1)' || { echo "Python 3.12 or newer is required" >&2; exit 1; }
command -v systemctl >/dev/null || { echo "systemd is required" >&2; exit 1; }

SRC="$(cd "$(dirname "$0")" 2>/dev/null && pwd || true)"
TMP=""
if [ ! -f "$SRC/bin/pc-panel" ]; then
    # running from curl: fetch the newest published release, or the main branch if there is none yet
    TMP="$(mktemp -d)"
    trap 'rm -rf "$TMP"' EXIT
    URL="$(curl -fsSL "https://api.github.com/repos/$REPO/releases/latest" 2>/dev/null | sed -n 's/.*"tarball_url": *"\([^"]*\)".*/\1/p' | head -n 1)"
    [ -n "$URL" ] || URL="https://github.com/$REPO/archive/refs/heads/main.tar.gz"
    echo "Downloading $URL"
    curl -fsSL "$URL" | tar -xz -C "$TMP" --strip-components=1
    SRC="$TMP"
fi

install -Dm755 "$SRC/bin/pc-panel" "$PREFIX/bin/pc-panel"
install -Dm644 "$SRC/share/pc-panel/page.html" "$PREFIX/share/pc-panel/page.html"
install -Dm755 "$SRC/lib/pc-panel/pc-panel-fans" "$PREFIX/lib/pc-panel/pc-panel-fans"
mkdir -p "$(dirname "$UNIT")"
sed "s|@BIN@|$PREFIX/bin/pc-panel|" "$SRC/systemd/pc-panel.service" > "$UNIT"
systemctl --user daemon-reload
systemctl --user enable pc-panel.service >/dev/null
systemctl --user restart pc-panel.service

if [ "$FANS" = yes ]; then
    echo "Enabling fan control (administrator password)"
    root sh -c "install -Dm755 '$SRC/lib/pc-panel/pc-panel-fans' $ROOT_LIB/pc-panel-fans \
        && sed 's|@LIB@|$ROOT_LIB|' '$SRC/systemd/pc-panel-fans.service' > /etc/systemd/system/pc-panel-fans.service \
        && install -m644 '$SRC/systemd/pc-panel-fans.path' /etc/systemd/system/pc-panel-fans.path \
        && $ROOT_LIB/pc-panel-fans --allow '$(id -un)' \
        && systemctl daemon-reload && systemctl enable --now pc-panel-fans.path && systemctl enable pc-panel-fans.service"
fi

echo
echo "PC Panel $("$PREFIX/bin/pc-panel" --version | cut -d' ' -f2) is running. Open http://127.0.0.1:47822"
[ "$FANS" = yes ] || echo "Fan control is off. To enable it later run this installer again with --fans"
