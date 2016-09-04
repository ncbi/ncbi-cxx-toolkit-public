Name:        ncbi-blast
Version:     BLAST_VERSION+
Release:     5
Source0:     %{name}-%{version}.tgz
Summary:     NCBI BLAST finds regions of similarity between biological sequences. 
Exclusiveos: linux
Group:       NCBI/BLAST
License:     Public Domain
BuildArch:   i686 x86_64
BuildRoot:   /var/tmp/%{name}-buildroot
Prefix:      /usr

AutoReqProv: no
Requires:    /usr/bin/perl  
Requires:    ld-linux-x86-64.so.2()(64bit)  
Requires:    ld-linux-x86-64.so.2(GLIBC_2.3)(64bit)  
Requires:    libbz2.so.1()(64bit)  
Requires:    libc.so.6()(64bit)  
Requires:    libc.so.6(GLIBC_2.2.5)(64bit)  
Requires:    libc.so.6(GLIBC_2.3)(64bit)  
Requires:    libc.so.6(GLIBC_2.3.2)(64bit)  
Requires:    libc.so.6(GLIBC_2.7)(64bit)  
Requires:    libdl.so.2()(64bit)  
Requires:    libdl.so.2(GLIBC_2.2.5)(64bit)  
Requires:    libgcc_s.so.1()(64bit)  
Requires:    libgcc_s.so.1(GCC_3.0)(64bit)  
Requires:    libm.so.6()(64bit)  
Requires:    libm.so.6(GLIBC_2.2.5)(64bit)  
Requires:    libnsl.so.1()(64bit)  
Requires:    libpthread.so.0()(64bit)  
Requires:    libpthread.so.0(GLIBC_2.2.5)(64bit)  
Requires:    libpthread.so.0(GLIBC_2.3.2)(64bit)  
Requires:    librt.so.1()(64bit)  
Requires:    libstdc++.so.6()(64bit)  
Requires:    libstdc++.so.6(CXXABI_1.3)(64bit)  
Requires:    libstdc++.so.6(CXXABI_1.3.1)(64bit)  
Requires:    libstdc++.so.6(GLIBCXX_3.4)(64bit)  
Requires:    libstdc++.so.6(GLIBCXX_3.4.5)(64bit)  
Requires:    libz.so.1()(64bit)  
Requires:    libz.so.1(ZLIB_1.2.0)(64bit)  
Requires:    perl(Archive::Tar)  
Requires:    perl(Digest::MD5)  
Requires:    perl(File::Temp)  
Requires:    perl(File::stat)  
Requires:    perl(Getopt::Long)  
Requires:    perl(List::MoreUtils)  
Requires:    perl(Net::FTP)  
Requires:    perl(Pod::Usage)  
Requires:    perl(constant)  
Requires:    perl(strict)  
Requires:    perl(warnings)  
Requires:    rpmlib(CompressedFileNames) <= 3.0.4-1
Requires:    rpmlib(FileDigests) <= 4.6.0-1
Requires:    rpmlib(PayloadFilesHavePrefix) <= 4.0-1
Requires:    rpmlib(PayloadIsXz) <= 5.2-1

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
