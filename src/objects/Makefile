MODULES = mim entrez2 general biblio medline medlars pub pubmed seqloc seqalign seqblock seqfeat seqres seqset seq submit proj mmdb1 mmdb2 mmdb3 cdd cn3d ncbimime access featdef objprt seqcode id1

#new_module = $(NCBI)/c++/scripts/new_module.sh
new_module = ../../../scripts/new_module.sh

all all_files sources makefiles directories purge_sources purge_all_sources purge_all_files:
	for i in $(MODULES); do \
		MAKE=$(MAKE); export MAKE; \
		( cd $$i && $(new_module) $$i $@; ) || exit 2; \
	done

all: clean_sources

clean_sources:
	for i in $(MODULES); do \
		rm -f $$i/$$i.files; \
	done

ASNFILES = $(MODULES:%=%/%.asn)

all.asn: $(ASNFILES)
	cat $(ASNFILES) >$@

