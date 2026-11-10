#!/bin/sh
# Builds pc-panel-VERSION-x86_64.AppImage. Needs make, a C compiler, the systemd headers and
# appimagetool in PATH (or APPIMAGETOOL=/path). It uses the python3 and libsystemd of the host.
set -eu
cd "$(dirname "$0")/../.."
V="$(sed -n 's/^VERSAO = "\(.*\)"/\1/p' bin/pc-panel)"
rm -rf AppDir
make DESTDIR=AppDir PREFIX=/usr install
cp packaging/appimage/AppRun packaging/appimage/pc-panel.desktop packaging/appimage/pc-panel.svg AppDir/
chmod +x AppDir/AppRun
ARCH=x86_64 ${APPIMAGETOOL:-appimagetool} AppDir "pc-panel-$V-x86_64.AppImage"
