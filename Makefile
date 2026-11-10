PREFIX ?= /usr
DESTDIR ?=
CC ?= cc
CFLAGS ?= -O2 -pipe
VERSION := $(shell sed -n 's/^VERSAO = "\(.*\)"/\1/p' bin/pc-panel)
ARCH := $(shell uname -m)

.PHONY: all install dist bundle check clean
all: bin/pc-panel-tray

# the tray icon: C on sd-bus (libsystemd), no GUI toolkit
bin/pc-panel-tray: tray/pc-panel-tray.c
	$(CC) $(CPPFLAGS) $(CFLAGS) -Wall -Wextra -Wno-format-truncation -DVERSION='"$(VERSION)"' -o $@ $< $(LDFLAGS) -lsystemd

install: bin/pc-panel-tray
	install -Dm755 bin/pc-panel $(DESTDIR)$(PREFIX)/bin/pc-panel
	install -Dm755 bin/pc-panel-tray $(DESTDIR)$(PREFIX)/bin/pc-panel-tray
	install -Dm644 share/pc-panel/page.html $(DESTDIR)$(PREFIX)/share/pc-panel/page.html
	install -Dm644 share/icons/hicolor/scalable/apps/pc-panel.svg $(DESTDIR)$(PREFIX)/share/icons/hicolor/scalable/apps/pc-panel.svg
	sed 's|@TRAY@|$(PREFIX)/bin/pc-panel-tray|' share/applications/pc-panel.desktop > pc-panel.desktop.tmp
	install -Dm644 pc-panel.desktop.tmp $(DESTDIR)$(PREFIX)/share/applications/pc-panel.desktop
	rm -f pc-panel.desktop.tmp
	install -Dm755 lib/pc-panel/pc-panel-fans $(DESTDIR)$(PREFIX)/lib/pc-panel/pc-panel-fans
	install -d $(DESTDIR)$(PREFIX)/lib/systemd/user $(DESTDIR)$(PREFIX)/lib/systemd/system
	sed 's|@BIN@|$(PREFIX)/bin/pc-panel|' systemd/pc-panel.service > $(DESTDIR)$(PREFIX)/lib/systemd/user/pc-panel.service
	sed 's|@LIB@|$(PREFIX)/lib/pc-panel|' systemd/pc-panel-fans.service > $(DESTDIR)$(PREFIX)/lib/systemd/system/pc-panel-fans.service
	install -m644 systemd/pc-panel-fans.path $(DESTDIR)$(PREFIX)/lib/systemd/system/pc-panel-fans.path
	install -Dm644 LICENSE $(DESTDIR)$(PREFIX)/share/licenses/pc-panel/LICENSE

# source tarball
dist:
	git archive --prefix=pc-panel-$(VERSION)/ -o pc-panel-$(VERSION).tar.gz HEAD

# source tarball plus the compiled tray icon, used by install.sh and the self-updater
bundle: bin/pc-panel-tray
	rm -rf bundle-tmp && mkdir -p bundle-tmp
	git archive --prefix=pc-panel-$(VERSION)/ HEAD | tar -x -C bundle-tmp
	cp bin/pc-panel-tray bundle-tmp/pc-panel-$(VERSION)/bin/
	tar -C bundle-tmp -czf pc-panel-$(VERSION)-$(ARCH).tar.gz pc-panel-$(VERSION)
	rm -rf bundle-tmp

check:
	python3 -c "import ast; [ast.parse(open(f).read()) for f in ('bin/pc-panel', 'lib/pc-panel/pc-panel-fans')]"
	sh -n install.sh

clean:
	rm -f bin/pc-panel-tray pc-panel-*.tar.gz
