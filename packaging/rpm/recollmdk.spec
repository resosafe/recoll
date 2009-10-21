%define name recoll
%define version 1.12.2
%define release  %mkrel 1

Name:           %{name}
Version:        %{version}
Release:        %{release}

Summary:	Desktop full text search tool with a qt gui
Source0:	http://www.recoll.org/%{name}-%{version}.tar.gz
URL:            http://www.recoll.org/
Group:          Applications/Databases

BuildRoot:      %{_tmppath}/%{name}-%{version}-%{release}-buildroot
License:	GPL
# We build with a static link to xapian to avoid this dependency
#Requires:	xapian-core

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
%{_datadir}/applications/recoll-searchgui.desktop
%{_datadir}/icons/hicolor/48x48/apps/recoll.png
%{_datadir}/%{name}
%{_mandir}/man1/recoll*
%{_mandir}/man5/recoll*

# ---------------------------------------------------------------------------

%changelog
* Mon Oct 19 2009 Jean-Francois Dockes <jean-francois.dockes@wanadoo.fr> 1.12.2-1
- Update to release 1.12.2
* Thu Jan 29 2009 Jean-Francois Dockes <jean-francois.dockes@wanadoo.fr> 1.12.0-1
- Update to release 1.12.0
* Mon Oct 13 2008 Jean-Francois Dockes <jean-francois.dockes@wanadoo.fr> 1.11.0-1
- Update to release 1.11.0
* Thu May 27 2008 Jean-Francois Dockes <jean-francois.dockes@wanadoo.fr> 1.10.2-1
- Update to release 1.10.2
* Thu Jan 31 2008 Jean-Francois Dockes <jean-francois.dockes@wanadoo.fr> 1.10.1-1
- Update to release 1.10.1
* Wed Nov 21 2007 Jean-Francois Dockes <jean-francois.dockes@wanadoo.fr> 1.10.0-1
- Update to release 1.10.0
* Tue Sep 11 2007 Jean-Francois Dockes <jean-francois.dockes@wanadoo.fr> 1.9.0-1
- Update to release 1.9.0
* Tue Mar 6 2007 Jean-Francois Dockes <jean-francois.dockes@wanadoo.fr> 1.8.1-1
- Update to release 1.8.1
* Mon Jan 15 2007 Jean-Francois Dockes <jean-francois.dockes@wanadoo.fr> 1.7.5-1
- Update to release 1.7.5
* Mon Jan 08 2007 Jean-Francois Dockes <jean-francois.dockes@wanadoo.fr> 1.7.3-1
- Update to release 1.7.3
* Tue Nov 28 2006 Jean-Francois Dockes <jean-francois.dockes@wanadoo.fr> 1.6.1-1
- Update to release 1.6.1
* Mon Nov 20 2006 Jean-Francois Dockes <jean-francois.dockes@wanadoo.fr> 1.5.11-1
- Update to release 1.5.11
* Mon Oct 2 2006 Jean-Francois Dockes <jean-francois.dockes@wanadoo.fr> 1.5.2-1
- Update to release 1.5.2
* Sun May 7 2006 Jean-Francois Dockes <jean-francois.dockes@wanadoo.fr> 1.4.3-1
- Update to release 1.4.3
* Fri Mar 31 2006 Jean-Francois Dockes <jean-francois.dockes@wanadoo.fr> 1.3.3-1
- Update to release 1.3.3
* Thu Feb  2 2006 Jean-Francois Dockes <jean-francois.dockes@wanadoo.fr> 1.2.2-1
- Update to release 1.2.2
* Thu Jan 10 2006 Jean-Francois Dockes <jean-francois.dockes@wanadoo.fr> 1.1.0-1
- Initial packaging
