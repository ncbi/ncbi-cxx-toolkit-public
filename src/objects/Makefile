# $Id$

MODULES = omssa access biblio blast blastdb blastxml cdd cn3d docsum biotree \
          entrez2 entrezgene featdef gbseq general id1 id2 insdseq \
          medlars medline mim mla mmdb1 mmdb2 mmdb3 ncbimime objprt \
          proj pub pubmed scoremat seq seqalign seqblock seqcode \
          seqfeat seqloc seqsplit seqres seqset submit taxon1 tinyseq \
          pcsubstance pcassay remap homologene seqedit seqtable seqtest valerr

new_module = $(builddir)/new_module.sh

all all_files sources makefiles directories purge_sources purge_all_sources purge_all_files:
	for i in $(MODULES); do \
	  if test -f "$$i/$$i.asn"; then \
	    MAKE=$(MAKE); export MAKE; \
	    ( cd $$i && $(new_module) $$i $@; ) \
	    || case "$(MFLAGS)" in *k*) ;; *) exit 2 ;; esac; \
	  else \
	    echo "Warning: $$i/$$i.asn not found (skipping)"; \
	  fi; \
	done

all: clean_sources

clean_sources:
	for i in $(MODULES); do \
		rm -f $$i/$$i.files; \
	done

ASNFILES = $(MODULES:%=%/%.asn)

all.asn: $(ASNFILES)
	cat $(ASNFILES) >$@

