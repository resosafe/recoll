%define name recoll
%define version 1.6.1
%define release 0

Name:           %{name}
Version:        %{version}
Release:        %{release}

Summary:	Desktop Full Text Search Tool with a QT Gui
Source0:	http://www.recoll.org/%{name}-%{version}.tar.gz
URL:            http://www.recoll.org/
Group:          Applications/Databases

BuildRoot:      %{_tmppath}/%{name}-%{version}-%{release}-buildroot
License:	GPL

# We build with a static link to xapian to avoid depending on xapian
BuildRequires: qt3-devel
Requires:      qt3

%description
Recoll is a personal full text search package for Linux, FreeBSD and
other Unix systems. It is based on a very strong backend (Xapian), for
which it provides an easy to use, feature-rich, easy administration
interface.

# ---------------------------------------------------------------------------

%prep
%setup -q

# ---------------------------------------------------------------------------

%build
[ -n "$QTDIR" ] || . %{_sysconfdir}/profile.d/qt.sh
%configure
make %{?_smp_mflags} static

# ---------------------------------------------------------------------------

%install
rm -rf $RPM_BUILD_ROOT
%makeinstall

# ---------------------------------------------------------------------------

%clean
rm -rf $RPM_BUILD_ROOT

# ---------------------------------------------------------------------------

%files
%defattr(-,root,root,-)
%{_bindir}/*
%{_datadir}/%{name}
%{_datadir}/applications/recoll.desktop
%{_datadir}/icons/hicolor/48x48/apps/recoll.png
%{_mandir}/man1/recoll*
%{_mandir}/man5/recoll*

# ---------------------------------------------------------------------------

%changelog
* Tue Nov 28 2006 Jean-Francois Dockes <jean-francois.dockes@wanadoo.fr> 1.6.1-1
- Update to release 1.6.1
* Mon Oct 2 2006 Jean-Francois Dockes <jean-francois.dockes@wanadoo.fr> 1.4.3-1
- Update to release 1.5.3
* Sun May 7 2006 Jean-Francois Dockes <jean-francois.dockes@wanadoo.fr> 1.4.3-1
- Update to release 1.4.3
* Fri Mar 31 2006 Jean-Francois Dockes <jean-francois.dockes@wanadoo.fr> 1.3.3-1
- Update to release 1.3.3
* Thu Feb  2 2006 Jean-Francois Dockes <jean-francois.dockes@wanadoo.fr> 1.2.2-1
- Update to release 1.2.2
* Thu Jan 10 2006 Jean-Francois Dockes <jean-francois.dockes@wanadoo.fr> 1.1.0-1
- Initial packaging
