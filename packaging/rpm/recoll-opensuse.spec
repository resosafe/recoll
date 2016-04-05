#
# spec file for package recoll
#
# Copyright (c) 2013 SUSE LINUX Products GmbH, Nuernberg, Germany.
#
# All modifications and additions to the file contributed by third parties
# remain the property of their copyright owners, unless otherwise agreed
# upon. The license for this file, and modifications and additions to the
# file, is the same license as for the pristine package itself (unless the
# license for the pristine package is not an Open Source License, in which
# case the license is the MIT License). An &quot;Open Source License&quot; is a
# license that conforms to the Open Source Definition (Version 1.9)
# published by the Open Source Initiative.

# Please submit bugfixes or comments via http://bugs.opensuse.org/
#


Name:           recoll
Version:        1.21.6
Release:        0
Summary:        Extended Search
License:        GPL-2.0+
Summary:        Extended Search
Group:          Productivity/Text/Utilities
Url:            http://www.lesbonscomptes.com/recoll/
Source:         http://www.lesbonscomptes.com/recoll/%{name}-%{version}.tar.gz
BuildRequires:  aspell-devel
%if 0%{?suse_version} > 1320
BuildRequires:  pkgconfig(Qt5Core)
BuildRequires:  pkgconfig(Qt5WebKit)
BuildRequires:  pkgconfig(Qt5WebKitWidgets)
BuildRequires:  pkgconfig(Qt5Xml)
BuildRequires:  pkgconfig(Qt5Network)
BuildRequires:  pkgconfig(Qt5PrintSupport)
%else
BuildRequires:  libQtWebKit-devel
BuildRequires:  libqt4-devel
%endif
BuildRequires:  bison
BuildRequires:  libxapian-devel
BuildRequires:  python-devel
BuildRequires:  update-desktop-files
# Recommends for the helpers.
Recommends:     antiword
Recommends:     poppler-tools
Recommends:     libwpd-tools
Recommends:     libxslt
Recommends:     catdoc
Recommends:     djvulibre
Recommends:     python-mutagen
Recommends:     exiftool
Recommends:     perl-Image-ExifTool
Recommends:     python-base
Recommends:     unrar
Recommends:     python-rarfile
Recommends:     python-PyCHM
Recommends:     unrtf
Recommends:     pstotext
# Default requires for recoll itself
Requires:       sed
Requires:       awk
Requires:       aspell
Requires:       python
Suggests:       kio_recoll
BuildRoot:      %{_tmppath}/%{name}-%{version}-build

%description
Recoll is a personal full text search tool for Unix/Linux.

It is based on the very strong Xapian back-end, for which it provides a feature-rich yet easy to use front-end with a QT graphical interface.

Recoll is free, open source, and licensed under the GPL. The current version is 1.12.0 .
Features:

* Easy installation, few dependencies. No database daemon, web server, desktop environment or exotic language necessary.
* Will run on most unix-based systems
* Qt-based GUI. Can use either Qt 3 or Qt 4.
* Searches most common document types, emails and their attachments.
* Powerful query facilities, with boolean searches, phrases, proximity, wildcards, filter on file types and directory tree.
* Multi-language and multi-character set with Unicode based internals.
* (more detail)

%prep
%setup -q

%build
%if 0%{?suse_version} > 1320
export QMAKE=qmake-qt5
%endif
%configure --with-inotify
make %{?_smp_mflags}

%install
make DESTDIR=%{buildroot} install %{?_smp_mflags}

%post -p /sbin/ldconfig

%postun -p /sbin/ldconfig

%files
%defattr(-,root,root)
%dir %{_libdir}/recoll
%dir %{_datadir}/appdata
%{_bindir}/recoll
%{_bindir}/recollindex
%{_datadir}/recoll
%{_datadir}/applications/recoll-searchgui.desktop
%{_datadir}/appdata/recoll.appdata.xml
%{_datadir}/icons/hicolor
%{_datadir}/pixmaps/recoll.png
%{_libdir}/recoll/librecoll.so.%{version}
%{_libdir}/python2.7/site-packages/recoll
%{_libdir}/python2.7/site-packages/Recoll-1.0-py2.7.egg-info
%{_mandir}/man1/recoll.1.gz
%{_mandir}/man1/recollq.1.gz
%{_mandir}/man1/recollindex.1.gz
%{_mandir}/man5/recoll.conf.5.gz

%changelog
* Tue Apr 05 2016 Jean-Francois Dockes <jfd@recoll.org> 1.21.6-0
- Recoll 1.21.6, esp. for the kde5 kio

