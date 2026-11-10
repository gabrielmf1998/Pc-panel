Name:           pc-panel
Version:        0.1.0
Release:        1%{?dist}
Summary:        Local web dashboard to monitor and control a Linux PC
License:        MIT
URL:            https://github.com/gabrielmf1998/pc-panel
Source0:        %{name}-%{version}.tar.gz
BuildArch:      noarch
BuildRequires:  make
BuildRequires:  systemd-rpm-macros
Requires:       python3 >= 3.12
Requires:       systemd

%description
PC Panel shows RAM, VRAM, graphics card, processor, network, programs,
temperatures, disks and system health on a local web page, and can control
fans on supported hardware. The service sleeps when the page is closed.

%prep
%autosetup

%build

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
%{_datadir}/pc-panel/
%{_prefix}/lib/pc-panel/
%{_userunitdir}/pc-panel.service
%{_unitdir}/pc-panel-fans.path
%{_unitdir}/pc-panel-fans.service

%changelog
* Tue Nov 10 2026 PC Panel contributors - 0.1.0-1
- First release
