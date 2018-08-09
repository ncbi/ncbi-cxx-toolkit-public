Name:        ncbi-magicblast
Version:     BLAST_VERSION
Release:     1
Source0:     %{name}-%{version}.tgz
Summary:     NCBI Magic-BLAST is a tool for mapping next-generation RNA or DNA sequencing runs against a genome or transcriptome. 
Exclusiveos: linux
Group:       NCBI/BLAST
License:     Public Domain
BuildArch:   i686 x86_64
BuildRoot:   /var/tmp/%{name}-buildroot
Prefix:      /usr

Requires:    ncbi-blast

%description
NCBI Magic-BLAST is a specialized variant of BLAST designed for mapping
next-generation RNA or DNA sequencing runs against a genome or transcriptome.

%prep 
%setup -q

%build
./configure
cd c++/*/build
%__make -f Makefile.flat

%install
%__mkdir_p $RPM_BUILD_ROOT/%_bindir
%__install -m755 c++/*/bin/magicblast $RPM_BUILD_ROOT/%_bindir

%clean
rm -rf $RPM_BUILD_ROOT

%files
%defattr(-,root,root)
%_bindir/*

%changelog
* Fri Aug 19 2016 <blast-help@ncbi.nlm.nih.gov>
- See ChangeLog file

