Name:        ncbi-blast
Version:     BLAST_VERSION+
Release:     1
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
./configure
cd c++/*/build
%__make -f Makefile.flat

%install
%__mkdir_p $RPM_BUILD_ROOT/%_bindir
cd c++/*/bin
%__install -m755 blastp blastn blastx tblastn tblastx psiblast rpsblast rpstblastn blast_formatter deltablast makembindex segmasker dustmasker windowmasker makeblastdb makeprofiledb blastdbcmd blastdb_aliastool convert2blastmask blastdbcheck legacy_blast.pl update_blastdb.pl $RPM_BUILD_ROOT/%_bindir

%clean
rm -rf $RPM_BUILD_ROOT

%files
%defattr(-,root,root)
%_bindir/*

%changelog
* Tue Mar 20 2012 <blast-help@ncbi.nlm.nih.gov>
- See release notes in http://www.ncbi.nlm.nih.gov/books/NBK131777
