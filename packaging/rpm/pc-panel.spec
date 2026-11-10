%global debug_package %{nil}
Name:           pc-panel
Version:        0.2.0
Release:        1
Summary:        Local web dashboard to monitor and control a Linux PC, with a tray icon
License:        MIT
URL:            https://github.com/gabrielmf1998/pc-panel
Source0:        %{name}-%{version}.tar.gz
BuildRequires:  make
BuildRequires:  gcc
BuildRequires:  systemd-devel
BuildRequires:  systemd-rpm-macros
Requires:       python3 >= 3.12
Requires:       systemd
Recommends:     (zenity or kdialog)

%description
PC Panel shows RAM, VRAM, graphics card, processor, network, programs,
temperatures, disks and system health on a local web page, and can control
fans on supported hardware. The service sleeps when the page is closed.
A small tray icon starts and stops it, changes its port and turns
start with system on or off.

%prep
%autosetup

%build
make CFLAGS="%{optflags}" LDFLAGS="%{build_ldflags}"

%install
make DESTDIR=%{buildroot} PREFIX=%{_prefix} install

%post
%systemd_post pc-panel-fans.path pc-panel-fans.service
%systemd_user_post pc-panel.service

%preun
%systemd_preun pc-panel-fans.path pc-panel-fans.service
%systemd_user_preun pc-panel.service

%postun
%systemd_postun pc-panel-fans.path pc-panel-fans.service

%files
%license LICENSE
%doc README.md
%{_bindir}/pc-panel
%{_bindir}/pc-panel-tray
%{_datadir}/applications/pc-panel.desktop
%{_datadir}/icons/hicolor/scalable/apps/pc-panel.svg
%{_datadir}/pc-panel/
%{_prefix}/lib/pc-panel/
%{_userunitdir}/pc-panel.service
%{_unitdir}/pc-panel-fans.path
%{_unitdir}/pc-panel-fans.service

%changelog
* Tue Nov 10 2026 PC Panel contributors - 0.2.0-1
- Tray icon: change port, start with system, quit
- Self-update for every install type

* Tue Nov 10 2026 PC Panel contributors - 0.1.0-1
- First release
