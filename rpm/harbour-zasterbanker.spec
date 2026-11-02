Name:       harbour-zasterbanker
Summary:    Personal finances for Sailfish OS
Version:    0.5.0
Release:    1
License:    GPLv3+
URL:        https://github.com/Wunderfitz/harbour-zasterbanker
Source0:    %{name}-%{version}.tar.bz2

# Everything the app links dynamically (Qt5Core/Gui/Qml/Quick/Sql,
# libsailfishapp, libsailfishsecrets, libstdc++) is picked up by rpmbuild's
# ELF dependency generator. AqBanking, Gwenhywfar, GnuTLS, Nettle, GMP,
# libgcrypt and libgpg-error are linked statically (see CLAUDE.md), so they
# show up in neither Requires nor Provides. What follows is what the
# generator cannot see.
Requires:   sailfishsilica-qt5 >= 0.10.9
Requires:   sailfishsecretsdaemon
Requires:   sailfishsecretsdaemon-secretsplugins-default

BuildRequires:  pkgconfig(sailfishapp) >= 1.0.2
BuildRequires:  pkgconfig(Qt5Core)
BuildRequires:  pkgconfig(Qt5Qml)
BuildRequires:  pkgconfig(Qt5Quick)
BuildRequires:  pkgconfig(Qt5Sql)
BuildRequires:  pkgconfig(sailfishsecrets)
# CONFIG += sailfishapp_i18n runs lupdate and lrelease during %%install
BuildRequires:  qt5-qttools-linguist
BuildRequires:  desktop-file-utils

%description
Zaster Banker manages your personal finances on Sailfish OS. It talks to
German banks via FinTS (formerly HBCI) with PIN/TAN, shows your accounts and
their balances, and keeps everything it stores encrypted on the phone.

%prep
%setup -q -n %{name}-%{version}

%build
%qmake5
%make_build

%install
%qmake5_install

desktop-file-install --delete-original       \
  --dir %{buildroot}%{_datadir}/applications \
  %{buildroot}%{_datadir}/applications/*.desktop

%files
%defattr(-,root,root,-)
%{_bindir}/%{name}
%{_datadir}/%{name}
%{_datadir}/applications/%{name}.desktop
%{_datadir}/icons/hicolor/*/apps/%{name}.png
