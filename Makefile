PREFIX ?= /usr
DESTDIR ?=
VERSION := $(shell sed -n 's/^VERSAO = "\(.*\)"/\1/p' bin/pc-panel)

.PHONY: install dist check
install:
	install -Dm755 bin/pc-panel $(DESTDIR)$(PREFIX)/bin/pc-panel
	install -Dm644 share/pc-panel/page.html $(DESTDIR)$(PREFIX)/share/pc-panel/page.html
	install -Dm755 lib/pc-panel/pc-panel-fans $(DESTDIR)$(PREFIX)/lib/pc-panel/pc-panel-fans
	install -d $(DESTDIR)$(PREFIX)/lib/systemd/user $(DESTDIR)$(PREFIX)/lib/systemd/system
	sed 's|@BIN@|$(PREFIX)/bin/pc-panel|' systemd/pc-panel.service > $(DESTDIR)$(PREFIX)/lib/systemd/user/pc-panel.service
	sed 's|@LIB@|$(PREFIX)/lib/pc-panel|' systemd/pc-panel-fans.service > $(DESTDIR)$(PREFIX)/lib/systemd/system/pc-panel-fans.service
	install -m644 systemd/pc-panel-fans.path $(DESTDIR)$(PREFIX)/lib/systemd/system/pc-panel-fans.path
	install -Dm644 LICENSE $(DESTDIR)$(PREFIX)/share/licenses/pc-panel/LICENSE

dist:
	git archive --prefix=pc-panel-$(VERSION)/ -o pc-panel-$(VERSION).tar.gz HEAD

check:
	python3 -c "import ast; [ast.parse(open(f).read()) for f in ('bin/pc-panel', 'lib/pc-panel/pc-panel-fans')]"
	sh -n install.sh
