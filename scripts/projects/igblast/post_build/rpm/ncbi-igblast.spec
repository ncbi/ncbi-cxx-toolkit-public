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
./configure
cd c++/*/build
%__make -f Makefile.flat

%install
%__mkdir_p $RPM_BUILD_ROOT/%_bindir
%__install -m755 c++/*/bin/igblast[pn] $RPM_BUILD_ROOT/%_bindir
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
/%_datarootdir/igblast/internal_data/mouse/mouse.ndm.imgt
/%_datarootdir/igblast/optional_file/human_gl.aux
/%_datarootdir/igblast/optional_file/human_gl.aux.testonly
/%_datarootdir/igblast/optional_file/mouse_gl.aux
/%_datarootdir/igblast/internal_data/mouse/mouse_V
/%_datarootdir/igblast/internal_data/mouse/mouse_V.nhr
/%_datarootdir/igblast/internal_data/mouse/mouse_V.nin
/%_datarootdir/igblast/internal_data/mouse/mouse_V.nog
/%_datarootdir/igblast/internal_data/mouse/mouse_V.nsd
/%_datarootdir/igblast/internal_data/mouse/mouse_V.nsi
/%_datarootdir/igblast/internal_data/mouse/mouse_V.nsq
/%_datarootdir/igblast/internal_data/mouse/mouse_V.phr
/%_datarootdir/igblast/internal_data/mouse/mouse_V.pin
/%_datarootdir/igblast/internal_data/mouse/mouse_V.pog
/%_datarootdir/igblast/internal_data/mouse/mouse_V.psd
/%_datarootdir/igblast/internal_data/mouse/mouse_V.psi
/%_datarootdir/igblast/internal_data/mouse/mouse_V.psq
/%_datarootdir/igblast/internal_data/mouse/mouse_V_prot
/%_datarootdir/igblast/internal_data/human/human.ndm.imgt
/%_datarootdir/igblast/internal_data/human/human.ndm.kabat
/%_datarootdir/igblast/internal_data/human/human.pdm.imgt
/%_datarootdir/igblast/internal_data/human/human.pdm.kabat
/%_datarootdir/igblast/internal_data/human/human_V
/%_datarootdir/igblast/internal_data/human/human_V.nhr
/%_datarootdir/igblast/internal_data/human/human_V.nin
/%_datarootdir/igblast/internal_data/human/human_V.nog
/%_datarootdir/igblast/internal_data/human/human_V.nsd
/%_datarootdir/igblast/internal_data/human/human_V.nsi
/%_datarootdir/igblast/internal_data/human/human_V.nsq
/%_datarootdir/igblast/internal_data/human/human_V.phr
/%_datarootdir/igblast/internal_data/human/human_V.pin
/%_datarootdir/igblast/internal_data/human/human_V.pog
/%_datarootdir/igblast/internal_data/human/human_V.psd
/%_datarootdir/igblast/internal_data/human/human_V.psi
/%_datarootdir/igblast/internal_data/human/human_V.psq
/%_datarootdir/igblast/internal_data/human/human_V_prot
/%_datarootdir/igblast/internal_data/mouse/mouse.ndm.kabat
/%_datarootdir/igblast/internal_data/mouse/mouse.pdm.imgt
/%_datarootdir/igblast/internal_data/mouse/mouse.pdm.kabat

%changelog
* Tue Mar 20 2012 Christiam Camacho <camacho@ncbi.nlm.nih.gov>
- See ChangeLog file

