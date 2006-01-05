%define name recoll
%define version 1.0.16
%define release 1

Name:           %{name}
Version:        %{version}
Release:        %{release}

Summary:	desktop full text search tool with a qt gui
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
%{_datadir}/%{name}

# ---------------------------------------------------------------------------

%changelog
* Thu Jan  5 2006 Jean-Francois Dockes <jean-francois.dockes@wanadoo.fr> 1.0.15-1
- Initial packaging
