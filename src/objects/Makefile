SUB_PROJ = general biblio medline medlars pub pubmed seqloc seqalign seqblock seqfeat seqres seqset seq submit proj mmdb1 mmdb2 mmdb3 cdd ncbimime access featdef objprt seqcode

new_module = ../../../scripts/new_module.sh

all sources clean_sources makefiles:
	for i in $(SUB_PROJ); do \
		( cd $$i && $(new_module) $$i $@; ) || exit 2; \
	done
