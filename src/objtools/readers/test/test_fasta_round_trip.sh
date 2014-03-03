#!/bin/sh
# $Id$

status=0
app=test_fasta_round_trip
data=${app}_data

test_case() {
    echo "* $app $*"
    echo
    $CHECK_EXEC $app "$@"
    st=$?
    echo
    if [ $st -eq 0 ]; then
	echo "... passed"
    else
	echo "... failed with status $st"
	status=$st
    fi
    echo
    echo "------------------------------------------------------------"
    echo
}

# test_case -in $data/empty.fsa # CMemoryFile fails
test_case -in $data/simple_nuc.fsa
test_case -in $data/simple_prot.fsa
# fNoSeqData | fNoUserObjs
test_case -in $data/id_normalization.fsa -inflags 0x80080 \
    -expected $data/id_normalization.fsa2
# fNoSeqData | fNoUserObjs
# test_case -in $data/title_symbols.fsa -inflags 0x80080 \
#     -expected $data/title_symbols.fsa2 # title descriptors differ too
# in:  fNoSeqData | fNoUserObjs
# out: fKeepGTSigns
test_case -in $data/title_symbols.fsa -inflags 0x80080 -outflags 16
test_case -in $data/reflow.fsa -expected $data/reflow.fsa2
# fAllSeqIDs | fNoUserObjs
test_case -in $data/merged_sequences.fsa -inflags 0x80040 \
    -expected $data/merged_sequences.fsa2
# fDLOptional | fNoUserObjs
test_case -in $data/no_defline.fsa -inflags 0x80200 \
    -expected $data/no_defline.fsa2
# fNoSeqData | fParseRawID | fNoUserObjs
test_case -in $data/raw_accessions.fsa -inflags 0x80480 \
    -expected $data/raw_accessions.fsa2
# fNoSeqData | fAddMods, fShowModifiers
test_case -in $data/modifiers.fsa -inflags 0x20080 -outflags 256
# in:  fNoSeqData | fAddMods | fNoUserObjs
# out: fShowModifiers
test_case -in $data/modifier_normalization.fsa -inflags 0xa0080 -outflags 256 \
    -expected $data/modifier_normalization.fsa2
# fNoSeqData | fNoUserObjs
test_case -in $data/modifier_normalization.fsa -inflags 0x80080
# fNoSeqData | fQuickIDCheck | fNoUserObjs
test_case -in $data/dubious_id.fsa -inflags 0x880080 \
    -expected $data/dubious_id.fsa2

# Can't usefully test range handling here.

# fParseGaps = 16, fLetterGaps = 0x40000 (1 << 18)
# test_case -in $data/gaps_one_dash.fsa -gapmode 0 -inflags 16
test_case -in $data/gaps_dashes.fsa -gapmode 1 -inflags 16
test_case -in $data/gaps_letters.fsa -inflags 0x40010
test_case -in $data/gaps_counted.fsa -gapmode 3 -inflags 16
test_case -in $data/gaps_unk.fsa -gapmode 3 -inflags 16

test_case -in $data/gaps_letters.fsa -gapmode 1 -inflags 0x40010 \
    -expected $data/gaps_dashes.fsa
test_case -in $data/gaps_counted.fsa -gapmode 1 -inflags 16 \
    -expected $data/gaps_dashes.fsa

test_case -in $data/gaps_dashes.fsa -inflags 0x40010 \
    -expected $data/gaps_letters.fsa
test_case -in $data/gaps_counted.fsa -inflags 0x40010 \
    -expected $data/gaps_letters.fsa

test_case -in $data/gaps_dashes.fsa -gapmode 3 -inflags 16 \
    -expected $data/gaps_counted.fsa
test_case -in $data/gaps_letters.fsa -gapmode 3 -inflags 0x40010 \
    -expected $data/gaps_counted.fsa

test_case -in $data/masking.fsa -inflags 0x40010 # fParseGaps | fLetterGaps

exit $status
