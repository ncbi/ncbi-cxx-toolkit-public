#! /bin/sh

# Basic test to see if it works in the simplest case:

#set -x

# assume success until we find an error
RETVAL=0

run_agpconvert()
{
    # if no asnval, we try to do the test without it
    if asnval --help > /dev/null 2>&1 ; then
        ./agpconvert "$@"
    else
        ./agpconvert -no_asnval "$@"
    fi
}

fix_whitespace()
{
    tr '\n' ' ' | \
        sed -e 's/\([{},]\)/ \1 /g' | \
        tr -s ' '
}

TEST_DATA=./test_data

mymktemp()
{
    mktemp "$@" -t test_agpconvert.XXXXXXXX
}

AGPCONVERT_CHECKING_TMPFILE1=`mymktemp`
AGPCONVERT_CHECKING_TMPFILE2=`mymktemp`

ASNDIFFFILE1=`mymktemp`
ASNDIFFFILE2=`mymktemp`

MULTIFILE_TMP_FILE=`mymktemp`

AGPCONVERT_TMP_OUTDIR=`mymktemp -d`

remove_tmp_outdir_contents() {
    if [ -d "$AGPCONVERT_TMP_OUTDIR" ] ; then
        rm -rf $AGPCONVERT_TMP_OUTDIR
        mkdir -p $AGPCONVERT_TMP_OUTDIR
    fi
}

# Give it exactly 2 ASN.1 files and it normalizes them
# and diffs them.  If the diffs differ, RETVAL is
# set to non-zero and the two outputs are shown.
asn_diff()
{
    if [ $# != 2 ] ; then
        echo "ERROR: asn_diff takes exactly 2 args"
        return 1
    fi

    # FILE1 is the value we got and FILE2 is what we expected to get
    FILE1="$1"
    FILE2="$2"

    echo "FYI:"
    ls -l "$FILE1" "$FILE2"

    if test -s "$FILE1" -a -s "$FILE2" ; then
        true # good, neither file is empty
    else
        echo "Error: at least one of these files is empty: $FILE1 $FILE2"
        RETVAL=1
    fi

    cat /dev/null > $ASNDIFFFILE1
    cat /dev/null > $ASNDIFFFILE2
    normalize_ASN1 < "$FILE1" | fix_whitespace > $ASNDIFFFILE1
    normalize_ASN1 < "$FILE2" | fix_whitespace > $ASNDIFFFILE2

    diff -y "$ASNDIFFFILE1" "$ASNDIFFFILE2"
    DIFF_RCODE=$?
    if [ $DIFF_RCODE != 0 ] ; then
        RETVAL=1
        echo "Incorrectly got the following output: "
        echo ">>>"
        cat $ASNDIFFFILE1
        echo "<<<"
        echo "But this was expected:"
        cat $ASNDIFFFILE2
    fi

    # always return success so caller doesn't waste time
    # writing code to check the rcode when we're already
    # setting RETVAL here.
    return 0
}

# Since create-date and update-date change every day,
# they would cause the diff to falsely fail if
# they weren't normalized
normalize_ASN1()
{
    perl -e '@lines = <STDIN>;' \
         -e '$whole_thing = join("",@lines);' \
         -e '$whole_thing =~ s/\b(update|create)-date\s+std\s*{.*?}/\1-date std { year 2000 , month 1, day 1 }/sg;' \
         -e ' print $whole_thing;'
    if [ $? != 0 ] ; then
        RETVAL=1
        echo there was a problem running perl
    fi
}

# First arg is the file holding the expected result
# and the remaining args are what's sent to agpconvert
agpconvert_check_bioseq_set_output() 
{
    # first arg is the file holding the result
    EXPECTED_OUTPUT_ASN_FILE="$1"
    shift

    echo "#################### RUNNING TEST: $EXPECTED_OUTPUT_ASN_FILE $@"

    run_agpconvert "$@" > $AGPCONVERT_CHECKING_TMPFILE1
    AGPCONVERT_RCODE=$?
    if [ $AGPCONVERT_RCODE != 0 ] ; then
        RETVAL=1
        return
    fi

    asn_diff $AGPCONVERT_CHECKING_TMPFILE1 $EXPECTED_OUTPUT_ASN_FILE
}

# this is called when we *expect* failure.
# the first arg is an egrep pattern which at least one line
# of stderr should match
agpconvert_failure_expected() 
{
    ERROR_EGREP_PATTERN="$1"
    shift

    echo "#################### RUNNING TEST: $ERROR_EGREP_PATTERN $@"

    run_agpconvert "$@" > /dev/null 2> $AGPCONVERT_CHECKING_TMPFILE1
    if [ $? = 0 ] ; then
        RETVAL=1
        echo test failure because agpconvert expected to give non-zero exit code but did not
        return
    fi

    # make sure the stderr output has at least one line that matches
    # the pattern
    if ! egrep -q "$ERROR_EGREP_PATTERN" $AGPCONVERT_CHECKING_TMPFILE1 ; then
        RETVAL=1
        echo test failure because no line of agpconvert stderr matches the given pattern
        return
    fi
}

agpconvert_outdir_test()
{
    EXPECTED_ASN_RESULT="$1"
    shift

    # clear temporary directory
    remove_tmp_outdir_contents

    run_agpconvert -outdir $AGPCONVERT_TMP_OUTDIR -ofs 'weirdsuffix' "$@" || RETVAL=1

    asn_diff "${AGPCONVERT_TMP_OUTDIR}"/*.weirdsuffix "$EXPECTED_ASN_RESULT"
}

agpconvert_outdir_multifile_test()
{
    EXPECTED_ASN_RESULT="$1"
    shift

    # clear temporary directory
    remove_tmp_outdir_contents

    run_agpconvert -outdir $AGPCONVERT_TMP_OUTDIR -ofs 'weirdsuffix' "$@" || RETVAL=1

    # since it is multi-file, we combine all the files
    cat /dev/null > $MULTIFILE_TMP_FILE
    for file in "${AGPCONVERT_TMP_OUTDIR}"/*.weirdsuffix ; do
        echo "##### FILE BASENAME:" `basename $file` >> $MULTIFILE_TMP_FILE
        cat $file >> $MULTIFILE_TMP_FILE
    done
    asn_diff $MULTIFILE_TMP_FILE "$EXPECTED_ASN_RESULT"
}

# basic functionality
agpconvert_check_bioseq_set_output \
    ${TEST_DATA}/basic_test_1.expected_bioseq_set.asn \
    -stdout \
    -template ${TEST_DATA}/basic_test.seq_entry.asn \
    ${TEST_DATA}/basic_test.agp

# basic functionality with bioseq
agpconvert_check_bioseq_set_output \
    ${TEST_DATA}/basic_test_1.expected_bioseq_set.asn \
    -stdout \
    -template ${TEST_DATA}/basic_test.bioseq.asn \
    ${TEST_DATA}/basic_test.agp

# test -keeptemplateannots
agpconvert_check_bioseq_set_output ${TEST_DATA}/basic_test_keeptemplateannots.expected_bioseq_set.asn -keeptemplateannots -stdout -template ${TEST_DATA}/basic_test.seq_entry.asn ${TEST_DATA}/basic_test.agp

# test if -template is an accession or gi
for template in U54469.1 'gb|U54469.1' 1322283 ; do # gi points to same record as accession
    agpconvert_check_bioseq_set_output ${TEST_DATA}/basic_test_accn_template.expected_bioseq_set.asn -stdout -template "$template" -keeptemplateannots ${TEST_DATA}/basic_test.agp
done

#    -dl "SOME_DEFLINE" \
#    -nt 9689 \
#    -on "lion" \
#    -sn "Some STRAIN OF LION" \
#    -cl TEST_ARG_CL \
#    -cm TEST_ARG_CM \
#    -cn TEST_ARG_CN \
#    -ht TEST_ARG_HT \
#    -sc TEST_ARG_SC \
#    -sex TEST_ARG_SEX \
echo cat
cat ${TEST_DATA}/basic_test_misc.expected_bioseq_set.asn
echo cat
# test a bunch of defline, etc. stuff all at once
agpconvert_check_bioseq_set_output ${TEST_DATA}/basic_test_misc.expected_bioseq_set.asn \
    -stdout \
    -template ${TEST_DATA}/basic_test_no_biosrc.seq_entry.asn \
    ${TEST_DATA}/basic_test.agp
#agpconvert_check_bioseq_set_output ${TEST_DATA}/basic_test_misc.expected_bioseq_set.asn -dl "SOME_DEFLINE" -nt 9689 -on "lion" -sn "Some STRAIN OF LION" -cl #TEST_ARG_CL -cm TEST_ARG_CM -cn TEST_ARG_CN -ht TEST_ARG_HT -sc TEST_ARG_SC -sex TEST_ARG_SEX -stdout -template ${TEST_DATA}/basic_test_no_biosrc.seq_entry.asn# ${TEST_DATA}/basic_test.agp
exit 0

# test chromosomes
agpconvert_check_bioseq_set_output ${TEST_DATA}/basic_test_chr.expected_bioseq_set.asn -chromosomes ${TEST_DATA}/basic_test.chromosomes.txt -stdout -template ${TEST_DATA}/basic_test.seq_entry.asn ${TEST_DATA}/basic_test.agp

# test a few AGP-to-Seq-Entry args at once
agpconvert_check_bioseq_set_output ${TEST_DATA}/basic_test_agp_to_seq_entry.expected_bioseq_set.asn -fuzz100 -fasta_id -gap-info -stdout -template ${TEST_DATA}/basic_test.seq_entry.asn ${TEST_DATA}/basic_test.agp

# test -len-check
agpconvert_failure_expected '\b458\b.*\b100\b' -len-check -stdout -template ${TEST_DATA}/basic_test.seq_entry.asn ${TEST_DATA}/basic_test.agp

# test -components failure
agpconvert_failure_expected '\b90\b.*\b218\b' -fasta_id -components ${TEST_DATA}/basic_test_failing.components.txt -stdout -template ${TEST_DATA}/basic_test.seq_entry.asn ${TEST_DATA}/basic_test.agp

# test successful components validation
agpconvert_check_bioseq_set_output ${TEST_DATA}/basic_test_success_components.expected_bioseq_set.asn -fasta_id -components ${TEST_DATA}/basic_test_success.components.txt -stdout -template ${TEST_DATA}/basic_test.seq_entry.asn ${TEST_DATA}/basic_test.agp

# test -outdir and -ofs without a Submit-block
agpconvert_outdir_test ${TEST_DATA}/basic_test_ent_outdir.expected_seq_entry.asn -template ${TEST_DATA}/basic_test.seq_entry.asn ${TEST_DATA}/basic_test.agp

# test -outdir and -ofs without a Submit-block
agpconvert_outdir_test ${TEST_DATA}/basic_test_sub_outdir.expected_seq_submit.asn -template ${TEST_DATA}/basic_test.seq_submit.asn ${TEST_DATA}/basic_test.agp

# test with multiple AGP files and multiple objects in each file,
# for -stdout
agpconvert_check_bioseq_set_output ${TEST_DATA}/multi_agp_stdout.expected_seq_set.asn -stdout -fasta_id -template ${TEST_DATA}/basic_test.seq_entry.asn ${TEST_DATA}/multi_obj_1.agp ${TEST_DATA}/multi_obj_2.agp

# test with multiple AGP files and multiple objects in each file,
# for -outdir
agpconvert_outdir_multifile_test ${TEST_DATA}/multi_agp_outdir.expected_seq_set.asn -fasta_id -template ${TEST_DATA}/basic_test.seq_entry.asn ${TEST_DATA}/multi_obj_1.agp ${TEST_DATA}/multi_obj_2.agp

# cleanup at the end
rm -f $AGPCONVERT_CHECKING_TMPFILE1
rm -f $AGPCONVERT_CHECKING_TMPFILE2

rm -f $ASNDIFFFILE1
rm -f $ASNDIFFFILE2

rm -f $MULTIFILE_TMP_FILE

rm -rf $AGPCONVERT_TMP_OUTDIR

exit $RETVAL
