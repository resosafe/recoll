Summary:        Desktop full text search tool with Qt GUI
Name:           recoll
Version:        1.21.6
Release:        1%{?dist}
Group:          Applications/Databases
License:        GPLv2+
URL:            http://www.lesbonscomptes.com/recoll/
Source0:        http://www.lesbonscomptes.com/recoll/recoll-%{version}.tar.gz
# Source10:       qmake-qt4.sh
BuildRequires:  aspell-devel
BuildRequires:  bison
BuildRequires:  desktop-file-utils
# kio
BuildRequires:  kdelibs4-devel
BuildRequires:  qt5-qtbase-devel
BuildRequires:  qt5-qtwebkit-devel
BuildRequires:  extra-cmake-modules
BuildRequires:  kf5-kio-devel
BuildRequires:  python2-devel
BuildRequires:  xapian-core-devel
BuildRequires:  zlib-devel
Requires:       xdg-utils

%description
Recoll is a personal full text search package for Linux, FreeBSD and
other Unix systems. It is based on a very strong back end (Xapian), for
which it provides an easy to use, feature-rich, easy administration
interface.

%package       kio
Summary:       KIO support for recoll
Group:         Applications/Databases
Requires:      %{name} = %{version}-%{release}

%description   kio
The recoll KIO slave allows performing a recoll search by entering an
appropriate URL in a KDE open dialog, or with an HTML-based interface
displayed in Konqueror.

%prep
%setup -q -n %{name}-%{version}
chmod 0644 utils/{conftree.cpp,conftree.h,debuglog.cpp,debuglog.h}

%build
CFLAGS="%{optflags}"; export CFLAGS
CXXFLAGS="%{optflags}"; export CXXFLAGS
LDFLAGS="%{?__global_ldflags}"; export LDFLAGS

# force use of custom/local qmake, to inject proper build flags (above)
#install -m755 -D %{SOURCE10} qmake-qt4.sh
export QMAKE=qmake-qt5

%configure
make %{?_smp_mflags}

%install
make install DESTDIR=%{buildroot} STRIP=/bin/true INSTALL='install -p'

desktop-file-install --delete-original \
  --dir=%{buildroot}/%{_datadir}/applications \
  %{buildroot}/%{_datadir}/applications/%{name}-searchgui.desktop

# use /usr/bin/xdg-open
rm -f %{buildroot}/usr/share/recoll/filters/xdg-open

# fix perms
chmod 0755 %{buildroot}/usr/share/recoll/filters/rclexecm.py
chmod 0755 %{buildroot}%{_libdir}/recoll/librecoll.so.*

# kio_recoll -kde5
(
mkdir kde/kioslave/kio_recoll/build && pushd kde/kioslave/kio_recoll/build
%cmake ..
make %{?_smp_mflags} VERBOSE=1
make install DESTDIR=%{buildroot}
popd
)

# kio_recoll -kde4
(
mkdir kde/kioslave/kio_recoll-kde4/build && \
      pushd kde/kioslave/kio_recoll-kde4/build
%cmake ..
make %{?_smp_mflags} VERBOSE=1
make install DESTDIR=%{buildroot}
popd
)

mkdir -p %{buildroot}%{_sysconfdir}/ld.so.conf.d
echo "%{_libdir}/recoll" > %{buildroot}%{_sysconfdir}/ld.so.conf.d/%{name}-%{_arch}.conf

%post
touch --no-create %{_datadir}/icons/hicolor
if [ -x %{_bindir}/gtk-update-icon-cache ] ; then
  %{_bindir}/gtk-update-icon-cache --quiet %{_datadir}/icons/hicolor
fi
if [ -x %{_bindir}/update-desktop-database ] ; then
  %{_bindir}/update-desktop-database &> /dev/null
fi
/sbin/ldconfig
exit 0

%postun
touch --no-create %{_datadir}/icons/hicolor 
if [ -x %{_bindir}/gtk-update-icon-cache ] ; then
  %{_bindir}/gtk-update-icon-cache --quiet %{_datadir}/icons/hicolor
fi
if [ -x %{_bindir}/update-desktop-database ] ; then
  %{_bindir}/update-desktop-database &> /dev/null
fi
/sbin/ldconfig
exit 0

%files
%doc COPYING ChangeLog README
%{_sysconfdir}/ld.so.conf.d/%{name}-%{_arch}.conf
%{_bindir}/%{name}
%{_bindir}/%{name}index
%{_datadir}/%{name}
%{_datadir}/appdata/%{name}.appdata.xml
%{_datadir}/applications/%{name}-searchgui.desktop
%{_datadir}/icons/hicolor/48x48/apps/%{name}.png
%{_datadir}/pixmaps/%{name}.png
%{_libdir}/recoll
%{python_sitearch}/recoll
%{python_sitearch}/Recoll*.egg-info
%{_mandir}/man1/%{name}.1*
%{_mandir}/man1/%{name}q.1*
%{_mandir}/man1/%{name}index.1*
%{_mandir}/man5/%{name}.conf.5*

%files kio
%{_libdir}/kde4/kio_recoll.so
%{_libdir}/qt5/plugins/kio_recoll.so
%{_datadir}/kde4/apps/kio_recoll/
%{_datadir}/kde4/services/recoll.protocol
%{_datadir}/kde4/services/recollf.protocol
%{_datadir}/kio_recoll/help.html
%{_datadir}/kio_recoll/welcome.html
%{_datadir}/kservices5/recoll.protocol
%{_datadir}/kservices5/recollf.protocol

%changelog
* Mon Jan 18 2016 Terje Rosten <terje.rosten@ntnu.no> - 1.21.4-1
- 1.21.4

* Sat Oct 31 2015 Terje Rosten <terje.rosten@ntnu.no> - 1.21.3-1
- 1.21.3

* Sat Oct 03 2015 Terje Rosten <terje.rosten@ntnu.no> - 1.21.2-1
- 1.21.2

* Thu Jun 18 2015 Fedora Release Engineering <rel-eng@lists.fedoraproject.org> - 1.20.6-2
- Rebuilt for https://fedoraproject.org/wiki/Fedora_23_Mass_Rebuild

* Tue May 05 2015 Terje Rosten <terje.rosten@ntnu.no> - 1.20.6-1
- 1.20.6
- Fixes bz#1218161 and bz#1204464

* Sat May 02 2015 Kalev Lember <kalevlember@gmail.com> - 1.20.5-3
- Rebuilt for GCC 5 C++11 ABI change

* Sat Apr 11 2015 Terje Rosten <terje.rosten@ntnu.no> - 1.20.5-2
- Add KIO subpackage

* Tue Apr 07 2015 Terje Rosten <terje.rosten@ntnu.no> - 1.20.5-1
- 1.20.5
- Include kio support (bz#1203257)

* Sat Jan 10 2015 Terje Rosten <terje.rosten@ntnu.no> - 1.20.1-1
- 1.20.1

* Sun Nov 09 2014 Terje Rosten <terje.rosten@ntnu.no> - 1.19.14p2-1
- 1.19.14p2

* Sun Aug 17 2014 Fedora Release Engineering <rel-eng@lists.fedoraproject.org> - 1.19.13-3
- Rebuilt for https://fedoraproject.org/wiki/Fedora_21_22_Mass_Rebuild

* Sun Jun 08 2014 Fedora Release Engineering <rel-eng@lists.fedoraproject.org> - 1.19.13-2
- Rebuilt for https://fedoraproject.org/wiki/Fedora_21_Mass_Rebuild

* Tue May 06 2014 Terje Rosten <terje.rosten@ntnu.no> - 1.19.13-1
- 1.19.13

* Mon Jan 20 2014 Terje Rosten <terje.rosten@ntnu.no> - 1.19.11-1
- 1.19.11

* Mon Nov 11 2013 Terje Rosten <terje.rosten@ntnu.no> - 1.19.9-1
- 1.19.9

* Tue Nov 05 2013 Terje Rosten <terje.rosten@ntnu.no> - 1.19.8-1
- 1.19.8

* Sun Aug 04 2013 Fedora Release Engineering <rel-eng@lists.fedoraproject.org> - 1.19.4-3
- Rebuilt for https://fedoraproject.org/wiki/Fedora_20_Mass_Rebuild

* Wed Jun 12 2013 Terje Rosten <terje.rosten@ntnu.no> - 1.19.4-2
- Fix filter setup

* Mon Jun 10 2013 Terje Rosten <terje.rosten@ntnu.no> - 1.19.4-1
- 1.19.4

* Thu Feb 14 2013 Fedora Release Engineering <rel-eng@lists.fedoraproject.org> - 1.18.1-2
- Rebuilt for https://fedoraproject.org/wiki/Fedora_19_Mass_Rebuild

* Mon Nov 19 2012 Terje Rosten <terje.rosten@ntnu.no> - 1.18.1-1
- 1.18.1

* Sat Jul 21 2012 Fedora Release Engineering <rel-eng@lists.fedoraproject.org> - 1.17.3-2
- Rebuilt for https://fedoraproject.org/wiki/Fedora_18_Mass_Rebuild

* Wed May 30 2012 Terje Rosten <terje.rosten@ntnu.no> - 1.17.3-1
- 1.17.3

* Sat Mar 31 2012 Terje Rosten <terje.rosten@ntnu.no> - 1.17.1-1
- 1.17.1

* Sun Mar 25 2012 Terje Rosten <terje.rosten@ntnu.no> - 1.17-1
- 1.17

* Tue Feb 28 2012 Fedora Release Engineering <rel-eng@lists.fedoraproject.org> - 1.16.2-3
- Rebuilt for c++ ABI breakage

* Wed Feb 15 2012 Terje Rosten <terje.rosten@ntnu.no> - 1.16.2-2
- Add patch to build with gcc 4.7

* Wed Feb 01 2012 Terje Rosten <terje.rosten@ntnu.no> - 1.16.2-1
- 1.16.2

* Sat Jan 14 2012 Fedora Release Engineering <rel-eng@lists.fedoraproject.org> - 1.16.1-3
- Rebuilt for https://fedoraproject.org/wiki/Fedora_17_Mass_Rebuild

* Mon Oct 24 2011 Terje Rosten <terje.rosten@ntnu.no> - 1.16.1-2
- Add patch to fix crash (bz #747472)

* Tue Oct 18 2011 Terje Rosten <terje.rosten@ntnu.no> - 1.16.1-1
- 1.16.1

* Tue May 24 2011 Terje Rosten <terje.rosten@ntnu.no> - 1.15.8-2
- add patch from upstream to fix crash.

* Sun May 08 2011 Terje Rosten <terje.rosten@ntnu.no> - 1.15.8-1
- 1.15.8

* Wed Feb 09 2011 Fedora Release Engineering <rel-eng@lists.fedoraproject.org> - 1.14.4-2
- Rebuilt for https://fedoraproject.org/wiki/Fedora_15_Mass_Rebuild

* Fri Jan 28 2011 Terje Rosten <terje.rosten@ntnu.no> - 1.14.4-1
- 1.14.4

* Mon Nov 15 2010 Terje Rosten <terje.rosten@ntnu.no> - 1.14.2-2
- Add patch to fix #631704

* Sun Nov  7 2010 Terje Rosten <terje.rosten@ntnu.no> - 1.14.2-1
- 1.14.2

* Thu Jul 15 2010 Terje Rosten <terje.rosten@ntnu.no> - 1.13.04-6
- add patch to build with xapian 1.2 (from J.F. Dockes, thanks)

* Sat Jul 10 2010 Terje Rosten <terje.rosten@ntnu.no> - 1.13.04-5
- use system xdg-open
- trim chagenlog

* Fri Jul  9 2010 Terje Rosten <terje.rosten@ntnu.no> - 1.13.04-4
- fix some review comments

* Mon May 10 2010 Terje Rosten <terje.rosten@ntnu.no> - 1.13.04-3
- use version macro in source url
- don't strip bins, giving non empty debuginfo package
- more explicit file listing
- try to preserve timestamps 
- use proper name for spec file
- fix changelog
- license seems to be GPLv2+
- include some %%doc files
- add buildroot tag
- some source add execute bit set
- explicit enable inotify
- add aspell-devel to bulldreq
- make with _smp_mflags seems to work
- add post scripts
- add patch to build gui with correct flags (ref: 338791)

* Sun May  9 2010  J.F. Dockes <jfd@recoll.org> 1.13.04-2
- bumped the release number to issue new rpms for fc10

* Sun May  9 2010  J.F. Dockes 1.13.04
- updated to recoll release 1.13.04 

* Fri Feb 12 2010 Terry Duell 1.13.02
- updated to release 1.13.02

* Tue Jan 12 2010 Terry Duell  1.13.01-3
- updated to fix Fedora desktop-file-install and install icon

* Sun Jan 10 2010 Jean-Francois Dockes <jfd@recoll.org> 1.13.01-2
- updated for recent fedoras: depend on xapian packages, use qt4

* Thu Jan 07 2010 Jean-Francois Dockes <jfd@recoll.org> 1.13.01-1
- update to release 1.13.01

* Wed Feb  1 2006 Jean-Francois Dockes <jfd@recoll.org> 1.2.0-1
- initial packaging
