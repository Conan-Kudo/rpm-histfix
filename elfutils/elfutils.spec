Summary: A collection of utilities and DSOs to handle compiled objects.
Name: elfutils
Version: 0.53
Release: 1
Copyright: GPL
Group: Development/Tools
URL: file://home/devel/drepper/
Source: elfutils-0.53.tar.gz

# ExcludeArch: xxx

BuildRoot: %{_tmppath}/%{name}-root
BuildRequires: gcc >= 3.2
BuildRequires: sharutils

%define _gnu %{nil}
%define _programprefix eu-

%description
Elfutils is a collection of utilities, including ld (a linker),
nm (for listing symbols from object files), size (for listing the
section sizes of an object or archive file), strip (for discarding
symbols), readline (the see the raw ELF file structures), and elflint
(to check for well-formed ELF files).  Also included are numerous
helper libraries which implement DWARF, ELF, and machine-specific ELF
handling.

%package devel
Summary: Development libraries to handle compiled objects.
Group: Development/Tools
Requires: elfutils = %{version}-%{release}
Conflicts: libelf-devel

%description devel
The elfutils-devel package containst he libraries to create
applications for handling compiled objects.  libelf allows you to
access the internals of the ELF object file format, so you can see the
different sections of an ELF file.  libebl provides some higher-level
ELF access functionality.  libdwarf provides access to the DWARF
debugging information.  libasm provides a programmable assembler
interface.

%prep
%setup -q
# %patch0 -p1 -b .jbj

%build
mkdir build-%{_target_platform}
cd build-%{_target_platform}
../configure \
  --prefix=%{_prefix} --exec-prefix=%{_exec_prefix} \
  --bindir=%{_bindir} --sbindir=%{_sbindir} --sysconfdir=%{_sysconfdir} \
  --datadir=%{_datadir} --includedir=%{_includedir} --libdir=%{_libdir} \
  --libexecdir=%{_libexecdir} --localstatedir=%{_localstatedir} \
  --sharedstatedir=%{_sharedstatedir} --mandir=%{_mandir} \
  --infodir=%{_infodir} --program-prefix=%{_programprefix} --enable-shared
cd ..

%install
rm -rf ${RPM_BUILD_ROOT}
mkdir -p ${RPM_BUILD_ROOT}%{_prefix}

cd build-%{_target_platform}
%makeinstall

chmod +x ${RPM_BUILD_ROOT}%{_prefix}/%{_lib}/lib*.so*
chmod +x ${RPM_BUILD_ROOT}%{_prefix}/%{_lib}/elfutils/lib*.so*

# The loadable modules need not exist in a form which can be linked with
rm ${RPM_BUILD_ROOT}%{_prefix}/%{_lib}/elfutils/lib*.a
rm ${RPM_BUILD_ROOT}%{_prefix}/%{_lib}/elfutils/lib*.la

cd ..

%clean
rm -rf ${RPM_BUILD_ROOT}

%post -p /sbin/ldconfig

%postun -p /sbin/ldconfig

%files
%defattr(-,root,root)
%doc README
%doc TODO
%doc libdwarf/AVAILABLE
%{_prefix}/bin/*
%{_prefix}/%{_lib}/lib*.so.*
%{_prefix}/%{_lib}/elfutils/lib*.so.*

%files devel
%defattr(-,root,root)
%{_prefix}/include/*
%{_prefix}/%{_lib}/lib*.a
%{_prefix}/%{_lib}/lib*.la
%{_prefix}/%{_lib}/lib*.so

%changelog
* Tue Oct 22 2002 Ulrich Drepper <drepper@redhat.com> 0.52
- Add libelf-devel to conflicts for elfutils-devel
* Mon Oct 21 2002 Ulrich Drepper <drepper@redhat.com> 0.50
- Split into runtime and devel package
* Fri Oct 18 2002 Ulrich Drepper <drepper@redhat.com> 0.49
- integrate into official sources
* Wed Oct 16 2002 Jeff Johnson <jbj@redhat.com> 0.46-1
- Swaddle.
