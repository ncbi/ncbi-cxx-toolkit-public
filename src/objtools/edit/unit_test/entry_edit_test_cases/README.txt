PLEASE MAKE SURE TO UPDATE THIS IF ANY STATEMENTS HERE GO OUT OF DATE

Each final file name here fits the following pattern:

${FUNC_TO_TEST}.${NAME_OF_TEST}.${PROCESSING_STAGE}.asn

If a file exists with the same name except extension ".asn_pp" instead
of ".asn", then the ".asn" file was generated *manually* 
via "cpp -w -P foo.asn_pp > foo.asn".  Please keep .asn_pp and .asn
files in sync in svn manually.  They are *not* automatically
kept in sync.

========================================
FUNC_TO_TEST:
This indicates what function will be run on the file.  For example,
"divvy" means to run the DivvyUpAlignments function on it.  Also, the
name must not be anything that could be valid for other parts of the
name.  For example, the NAME_OF_TEST can't be "divvy" because that
could also be a FUNC_TO_TEST.

========================================
NAME_OF_TEST:
This is free form but you must limit characters to [A-Za-z0-9_]

========================================
PROCESSING_STAGE:
This differs depending on the function you're testing.  A
PROCESSING_STAGE is mandatory for a given FUNC_TO_TEST unless noted
otherwise.  For all .asn files, we use text ASN.1

For FUNC_TO_TEST "divvy" (C++ func: DivvyUpAlignments),
PROCESSING_STAGE could be:
- "input":  This is the Seq-entry of type Bioseq-set.  All its direct
children are fed into the divvy func.
- "output_expected": This is the Seq-entry we expect to get back.  The
  test fails if it doesn't match.

For FUNC_TO_TEST "segregate" (C++ func: SegregateSetsByBioseqList),
PROCESSING_STAGE could be:
- "input_entry": This is the Seq-entry that we're going to segregate.
  The bioseqs that will be given as the second arg of
  SegregateSetsByBioseqList are found as the ones that are marked with
  Seqdesc user object of class "objtools.edit.unit_test.segregate".
  The other fields of that user object don't matter and will be
  ignored, so you might use them to make comments about why that
  bioseq is there, etc.
- "output_expected": This is the Seq-entry we expect to get back.  The
  test fails if it doesn't match.

========================================

