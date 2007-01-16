Name:           recoll
Version:        1.7.5
Release:        1%{?dist}
Summary:        Desktop full text search tool with a qt gui

Group:          Applications/Databases
License:        GPL
URL:            http://www.recoll.org/
Source0:        http://www.recoll.org/recoll-1.7.5.tar.gz 
BuildRoot:      %{_tmppath}/%{name}-%{version}-%{release}-root-%(%{__id_u} -n)

# Not sure how easy it is to find a xapian-core rpm. Will be easier to
# build by hand for many. Run time uses a static link to xapian, doesnt
# depend on libxapian.so
BuildRequires:  qt-devel
Requires:       qt

%description
Recoll is a personal full text search package for Linux, FreeBSD and
other Unix systems. It is based on a very strong backend (Xapian), for
which it provides an easy to use, feature-rich, easy administration
interface.

%prep
%setup -q

%build
[ -n "$QTDIR" ] || . %{_sysconfdir}/profile.d/qt.sh
%configure
make %{?_smp_mflags} static

%install
rm -rf $RPM_BUILD_ROOT
%makeinstall

%clean
rm -rf $RPM_BUILD_ROOT

%files
%defattr(-,root,root,-)
%{_bindir}/*
%{_datadir}/%{name}
%{_datadir}/applications/recoll.desktop
%{_datadir}/icons/hicolor/48x48/apps/recoll.png
%{_mandir}/man1/recoll*
%{_mandir}/man5/recoll*
%doc


%changelog
* Mon Jan 15 2007 Jean-Francois Dockes <jean-francois.dockes@wanadoo.fr> 1.7.5-1
- Update to release 1.7.5
* Mon Jan 08 2007 Jean-Francois Dockes <jean-francois.dockes@wanadoo.fr> 1.7.3-1
- Update to release 1.7.3
* Tue Nov 28 2006 Jean-Francois Dockes <jean-francois.dockes@wanadoo.fr> 1.6.1-1
- Update to release 1.6.0
* Mon Nov 20 2006 Jean-Francois Dockes <jean-francois.dockes@wanadoo.fr> 1.5.11-1
- Update to release 1.5.11
* Mon Oct 2 2006 Jean-Francois Dockes <jean-francois.dockes@wanadoo.fr> 1.5.2-1
- Update to release 1.5.2
* Fri Mar 31 2006 Jean-Francois Dockes <jean-francois.dockes@wanadoo.fr> 1.3.2-1
- Update to release 1.3.1
* Wed Feb  1 2006 Jean-Francois Dockes <jean-francois.dockes@wanadoo.fr> 1.2.0-1
- Initial packaging
