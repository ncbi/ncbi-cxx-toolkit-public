Name:        ncbi-cobalt
Version:     BLAST_VERSION
Release:     1
Source0:     %{name}-%{version}.tgz
Summary:     NCBI COBALT is a tool for multiple protein sequence alignment
Exclusiveos: linux
Group:       NCBI/BLAST
License:     Public Domain
BuildArch:   i686 x86_64
BuildRoot:   /var/tmp/%{name}-buildroot
Prefix:      /usr

%description
NCBI Constrain-Based Alignment Tool (COBALT) aligns multiple protein
sequences. The program uses amino acid frequencies from conserved domain
database (CDD) and collection of pairwise constraints derived
from CDD, protein motifs, and local sequence similarity using RPS-BLAST,
BLASTP, and PHI-BLAST. The constraints are incorporated into the multiple
alignment.

%prep 
%setup -q

%build
./configure
cd c++/*/build
%__make -f Makefile.flat

%install
%__mkdir_p $RPM_BUILD_ROOT/%_bindir
%__install -m755 c++/*/bin/cobalt $RPM_BUILD_ROOT/%_bindir

%clean
rm -rf $RPM_BUILD_ROOT

%files
%defattr(-,root,root)
%_bindir/*

%changelog
* Fri Aug 19 2016 <blast-help@ncbi.nlm.nih.gov>
- See ChangeLog file

