Name:        ncbi-blast
Version:     BLAST_VERSION+
Release:     4
Source0:     %{name}-%{version}.tgz
Summary:     NCBI BLAST finds regions of similarity between biological sequences. 
Exclusiveos: linux
Group:       NCBI/BLAST
License:     Public Domain
BuildArch:   i686 x86_64
BuildRoot:   /var/tmp/%{name}-buildroot
Prefix:      /usr

%description
The NCBI Basic Local Alignment Search Tool (BLAST) finds regions of
local similarity between sequences. The program compares nucleotide or
protein sequences to sequence databases and calculates the statistical
significance of matches. BLAST can be used to infer functional and
evolutionary relationships between sequences as well as help identify
members of gene families.

%prep 
%setup -q

%build
./configure --without-debug --with-mt --with-flat-makefile \
            --with-build-root=ReleaseMT
cd c++/ReleaseMT/build
%__make -f Makefile.flat

%install
%__mkdir_p $RPM_BUILD_ROOT/%_bindir
cd c++/ReleaseMT
%__install -m755 bin/* $RPM_BUILD_ROOT/%_bindir

%clean
rm -rf $RPM_BUILD_ROOT

%files
%defattr(-,root,root)
%_bindir/*

%changelog
* Mon Jul 21 2008 Christiam Camacho <camacho@ncbi.nlm.nih.gov>
- First release

