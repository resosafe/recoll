Name:           recoll
Version:        1.13.02
Release:        1%{?dist}
Summary:        Desktop full text search tool with a qt gui

Group:          Applications/Databases
License:        GPL
URL:            http://www.recoll.org/
Source0:        http://www.recoll.org/recoll-1.13.02.tar.gz 
BuildRoot:      %{_tmppath}/%{name}-%{version}-%{release}-root-%(%{__id_u} -n)

# Not sure how easy it is to find a xapian-core rpm. Will be easier to
# build by hand for many. Run time uses a static link to xapian, doesnt
# depend on libxapian.so
BuildRequires:  qt-devel xapian-core-devel zlib-devel desktop-file-utils
Requires:       qt xapian-core-libs zlib

%description
Recoll is a personal full text search package for Linux, FreeBSD and
other Unix systems. It is based on a very strong backend (Xapian), for
which it provides an easy to use, feature-rich, easy administration
interface.

%prep
%setup -q

%build
QMAKE=qmake-qt4 
export QMAKE
%configure
make %{?_smp_mflags}

%install
rm -rf %{buildroot}
make install DESTDIR=%{buildroot}

desktop-file-install --delete-original \
  --dir=%{buildroot}/%{_datadir}/applications \
  %{buildroot}/%{_datadir}/applications/%{name}-searchgui.desktop



%clean
rm -rf %{buildroot}

%files
%defattr(-,root,root,-)
%{_bindir}/*
%{_datadir}/%{name}
%{_datadir}/applications/%{name}-searchgui.desktop
%{_datadir}/pixmaps/%{name}.png
%{_datadir}/icons/hicolor/48x48/apps/%{name}.png
%{_mandir}/man1/recoll*
%{_mandir}/man5/recoll*
%doc


%changelog
* Fri Feb 12 2010 Terry Duell 1.13.02
- updated to release 1.13.02
* Mon Jan 12 2010 Terry Duell 1.13.01-3
- rpm spec file updated to fix Fedora desktop-file-install and install icon
* Sun Jan 10 2010 Jean-Francois Dockes <jfd@recoll.org> 1.13.01-2
- Rpm Spec file updated for recent fedoras: depend on xapian packages, use qt4
* Thu Jan 07 2010 Jean-Francois Dockes <jfd@recoll.org> 1.13.01-1
- Update to release 1.13.01
* Thu Dec 10 2009 Jean-Francois Dockes <jfd@recoll.org> 1.12.4-1
- Update to release 1.12.4
* Thu Jan 29 2009 Jean-Francois Dockes <jfd@recoll.org> 1.12.0-1
- Update to release 1.12.0
* Mon Oct 13 2008 Jean-Francois Dockes <jfd@recoll.org> 1.11.0-1
- Update to release 1.11.0
* Fri Sep 12 2008 Jean-Francois Dockes <jfd@recoll.org> 1.10.6-1
- Update to release 1.10.6
* Thu May 27 2008 Jean-Francois Dockes <jfd@recoll.org> 1.10.2-1
- Update to release 1.10.2
* Thu Jan 31 2008 Jean-Francois Dockes <jfd@recoll.org> 1.10.1-1
- Update to release 1.10.1
* Wed Nov 21 2007 Jean-Francois Dockes <jfd@recoll.org> 1.10.0-1
- Update to release 1.10.0
* Tue Sep 11 2007 Jean-Francois Dockes <jfd@recoll.org> 1.9.0-1
- Update to release 1.9.0
* Tue Mar 6 2007 Jean-Francois Dockes <jfd@recoll.org> 1.8.1-1
- Update to release 1.8.1
* Mon Jan 15 2007 Jean-Francois Dockes <jfd@recoll.org> 1.7.5-1
- Update to release 1.7.5
* Mon Jan 08 2007 Jean-Francois Dockes <jfd@recoll.org> 1.7.3-1
- Update to release 1.7.3
* Tue Nov 28 2006 Jean-Francois Dockes <jfd@recoll.org> 1.6.1-1
- Update to release 1.6.0
* Mon Nov 20 2006 Jean-Francois Dockes <jfd@recoll.org> 1.5.11-1
- Update to release 1.5.11
* Mon Oct 2 2006 Jean-Francois Dockes <jfd@recoll.org> 1.5.2-1
- Update to release 1.5.2
* Fri Mar 31 2006 Jean-Francois Dockes <jfd@recoll.org> 1.3.2-1
- Update to release 1.3.1
* Wed Feb  1 2006 Jean-Francois Dockes <jfd@recoll.org> 1.2.0-1
- Initial packaging
