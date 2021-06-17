#
# Makefile:  /home/kornbluh/c++.opt/src/objtools/readers/unit_test/unit_test_fasta_reader/Makefile.unit_test_agp_seq_entry.app
#

###  BASIC PROJECT SETTINGS
APP = unit_test_agp_seq_entry
SRC = unit_test_agp_seq_entry

CPPFLAGS = $(ORIG_CPPFLAGS) $(BOOST_INCLUDE)

LIB  = $(OBJREAD_LIBS) seqset $(SEQ_LIBS) pub medline biblio general xser \
    xutil test_boost xncbi
LIBS = $(ORIG_LIBS)

REQUIRES = Boost.Test.Included

CHECK_CMD =
CHECK_COPY = agp_seq_entry_test_cases

WATCHERS = ucko kachalos drozdov

