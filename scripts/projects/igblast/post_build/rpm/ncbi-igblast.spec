Name:        ncbi-igblast
Version:     BLAST_VERSION
Release:     1
Source0:     %{name}-%{version}.tgz
Summary:     NCBI igBLAST finds regions of similarity between biological sequences. 
Exclusiveos: linux
Group:       NCBI/BLAST
License:     Public Domain
BuildArch:   i686 x86_64
BuildRoot:   /var/tmp/%{name}-buildroot
Prefix:      /usr

%description
Immunoglobulin BLAST (IgBLAST), a specialized variant of BLAST that is designed
for the analysis of immunoglobulin sequence data.

%prep 
%setup -q

%build
./configure || ./configure --manifest-config=Linux64-Alma:gcc
cd c++/*/build
%__make -f Makefile.flat

%install
%__mkdir_p $RPM_BUILD_ROOT/%_bindir
%__install -m755 c++/*/bin/igblast[pn] c++/*/bin/edit_imgt_file.pl $RPM_BUILD_ROOT/%_bindir
%__install -m755 c++/*/bin/igblast[pn] c++/*/bin/makeogrannote.py $RPM_BUILD_ROOT/%_bindir
%__install -m755 c++/*/bin/igblast[pn] c++/*/bin/makeogrdb.py $RPM_BUILD_ROOT/%_bindir
%__mkdir_p $RPM_BUILD_ROOT/%{_prefix}/share/igblast
cp -R c++/src/app/igblast/internal_data $RPM_BUILD_ROOT/%{_prefix}/share/igblast
cp -R c++/src/app/igblast/optional_file $RPM_BUILD_ROOT/%{_prefix}/share/igblast

%clean
rm -rf $RPM_BUILD_ROOT

%files
%defattr(-,root,root)
%_bindir/*
%_prefix/share/igblast/optional_file
%_prefix/share/igblast/internal_data

%changelog
* Tue Mar 20 2012 <blast-help@ncbi.nlm.nih.gov>
- See ChangeLog file

