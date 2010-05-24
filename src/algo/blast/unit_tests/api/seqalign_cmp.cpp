/*  $Id$
 * ===========================================================================
 *
 *                            PUBLIC DOMAIN NOTICE
 *               National Center for Biotechnology Information
 *
 *  This software/database is a "United States Government Work" under the
 *  terms of the United States Copyright Act.  It was written as part of
 *  the author's official duties as a United States Government employee and
 *  thus cannot be copyrighted.  This software/database is freely available
 *  to the public for use. The National Library of Medicine and the U.S.
 *  Government have not placed any restriction on its use or reproduction.
 *
 *  Although all reasonable efforts have been taken to ensure the accuracy
 *  and reliability of the software and data, the NLM and the U.S.
 *  Government do not and cannot warrant the performance or results that
 *  may be obtained by using this software or data. The NLM and the U.S.
 *  Government disclaim all warranties, express or implied, including
 *  warranties of performance, merchantability or fitness for any particular
 *  purpose.
 *
 *  Please cite the author in any work or product based on this material.
 *
 * ===========================================================================
 *
 * Author: Christiam Camacho
 *
 */

/** @file seqalign_cmp.cpp
 * API to compare CSeq-aligns produced by BLAST
 */

#include <ncbi_pch.hpp>
#include "seqalign_cmp.hpp"
#ifdef NCBI_OS_IRIX
#include <math.h>
#else
#include <cmath>
#endif

// Object includes
#include <serial/serial.hpp>
#include <serial/iterator.hpp>
#include <objects/seqalign/Seq_align_set.hpp>

BEGIN_SCOPE(ncbi)
USING_SCOPE(objects);
BEGIN_SCOPE(blast)
BEGIN_SCOPE(qa)

//#define VERBOSE_DEBUG

/// BEGIN: Debugging functions 
template <class Container>
void s_PrintContainer(ofstream& out, const Container& c)
{
#if defined(VERBOSE_DEBUG)
    if ( c.empty() ) {
        out << "{}";
        return;
    }

    typename Container::const_iterator itr = c.begin();
    out << "{ " << *itr;
    for (; itr != c.end(); ++itr) {
        out << ", " << *itr;
    }
    out << " }";
#endif
}


#if defined(VERBOSE_DEBUG)
static void
s_PrintNeutralSeqAlign(ofstream& out, const SeqAlign& alignment)
{
    out << "SeqAlign::score = " << alignment.score << endl
        << "SeqAlign::num_ident = " << alignment.num_ident << endl
        << "SeqAlign::evalue = " << alignment.evalue << endl
        << "SeqAlign::bit_score = " << alignment.bit_score << endl
        << "SeqAlign::match = " << alignment.match << endl
        << "SeqAlign::query_strand = " << alignment.query_strand << endl
        << "SeqAlign::subject_strand = " << alignment.subject_strand << endl
        << "SeqAlign::GetNumSegments() = " << alignment.GetNumSegments()
        << endl << "SeqAlign::starts[" << alignment.starts.size()
        << "] = " << endl;
    s_PrintContainer(out, alignment.starts);
    out << endl << "SeqAlign::lengths[" << alignment.lengths.size() 
        << "] = " << endl;
    s_PrintContainer(out, alignment.lengths);
    out << endl;
}
#endif

static void 
s_PrintTSeqAlignSet(const string& fname, const TSeqAlignSet& neutral_seqaligns)
{
#if defined(VERBOSE_DEBUG)
    ofstream out(fname.c_str());
    if (!out) {
        throw runtime_error("Failed to open " + fname);
    }

    int index = 0;
    ITERATE(TSeqAlignSet, alignment, neutral_seqaligns) {
        out << "SeqAlign # " << ++index << endl;
        s_PrintNeutralSeqAlign(out, *alignment);
    }
#endif
}
/// END: Debugging functions 

CSeqAlignCmp::CSeqAlignCmp(const TSeqAlignSet& ref,
                           const TSeqAlignSet& test,
                           const CSeqAlignCmpOpts& opts)
: m_Ref(ref), m_Test(test), m_Opts(opts)
{
}

bool
CSeqAlignCmp::Run(string* errors) 
{
    bool retval = true;

    s_PrintTSeqAlignSet("old.neutral.txt", m_Ref);
    s_PrintTSeqAlignSet("new.neutral.txt", m_Test);

    // FIXME: add HSP matching logic here (i.e.: go through all SeqAligns in
    // reference set and match them with SeqAligns in test set. The criteria
    // used in seqaligndiff was that the least number of diffs between a given
    // pair of SeqAligns would indicate a match).

    if (m_Ref.size() != m_Test.size()) {
        if (errors) {
            (*errors) += "Different number of alignments:\n";
            (*errors) += NStr::IntToString(m_Ref.size()) + " vs. ";
            (*errors) += NStr::IntToString(m_Test.size()) + "\n";
        }
        retval = false;
    }

    // Temporary fix to deal with uneven number of alignments
    const TSeqAlignSet::size_type kMaxSize = min(m_Ref.size(), m_Test.size());
    for (TSeqAlignSet::size_type i = 0; i < kMaxSize; i++) {
        if (x_CompareOneAlign(&m_Ref[i], &m_Test[i], i+1, errors) > 0) {
            retval = false;
        }
    }
    return retval;
}

/// Interface class to hold values and to determine whether some difference 
/// in these values should be reported or not
template <class T>
class CValueHolder {
public:
    CValueHolder(string& field_name,
                 T reference_value,
                 T test_value,
                 T invalid_value,
                 T max_diff_value) 
    : m_FieldName(field_name), m_Ref(reference_value), m_Test(test_value), 
      m_Invalid(invalid_value), m_MaxDiff(max_diff_value) {}

    virtual ~CValueHolder() {}
    virtual bool ReportDiffs() const = 0;

    string& GetFieldName() const { return m_FieldName; }
    T GetReference() const { return m_Ref; }
    T GetTest() const { return m_Test; }
    T GetInvalidValue() const { return m_Invalid; }
    T GetMaximumAcceptableDiff() const { return m_MaxDiff; }

protected:
    string& m_FieldName;
    T m_Ref;
    T m_Test;
    T m_Invalid;
    T m_MaxDiff;
};

class CIntValueHolder : public CValueHolder<int> {
public:
    CIntValueHolder(string& field_name,
                    int reference_value, 
                    int test_value, 
                    int max_diff_value = 0,
                    int invalid_value = kInvalidIntValue)
    : CValueHolder<int>(field_name, reference_value, test_value, 
                        invalid_value, max_diff_value), 
    m_Diff(std::abs(reference_value - test_value))
    {}

    virtual bool ReportDiffs() const {
        return (m_Diff > GetMaximumAcceptableDiff());
    }

private:
    int m_Diff;
};

class CDoubleValueHolder : public CValueHolder<double> {
public:
    CDoubleValueHolder(string& field_name,
                       double reference_value,
                       double test_value,
                       double max_diff_value = 0.0,
                       double invalid_value = kInvalidDoubleValue)
    : CValueHolder<double>(field_name, reference_value, test_value, 
                           invalid_value, max_diff_value), 
    m_Diff(std::fabs(reference_value - test_value) / reference_value)
    {}

    virtual bool ReportDiffs() const {
        return (m_Diff > GetMaximumAcceptableDiff());
    };
    
private:
    double m_Diff;
};

/// Template wrapper around NStr::XToString functions, where X is a data type
template <class T>
string s_ToString(T value) { return "<unknown type>"; }
template <>
string s_ToString(int value) { return NStr::IntToString(value); }
template <>
string s_ToString(double value) { 
    return NStr::DoubleToString(value, 5, NStr::fDoubleScientific);
}

/** Compare values in the CValueHolder object.
 * @param aln_num Number which identifies this alignment, used in conjunction
 * with errors string to produce human readable output [in]
 * @param errors string to which errors will be appended
 * @return false if values differ, true otherwise
 */
template <class T>
bool s_CompareValues(const CValueHolder<T>& value_holder,
                     int aln_num = 0, 
                     string* errors = NULL)
{
    if (value_holder.GetReference() == value_holder.GetInvalidValue() && 
        value_holder.GetTest() != value_holder.GetInvalidValue()) {
        if (errors) {
            (*errors) += "align " + s_ToString(aln_num) + ": ";
            (*errors) += value_holder.GetFieldName() + " present\n";
        }
        return false;
    } else if (value_holder.GetReference() != value_holder.GetInvalidValue() && 
               value_holder.GetTest() == value_holder.GetInvalidValue()) {
        if (errors) {
            (*errors) += "align " + s_ToString(aln_num) + ": ";
            (*errors) += value_holder.GetFieldName() + " absent\n";
        }
        return false;
    } else if (value_holder.GetReference() != value_holder.GetInvalidValue() && 
               value_holder.GetTest() != value_holder.GetInvalidValue()) {
        if (value_holder.ReportDiffs()) {
            if (errors) {
                (*errors) += "align " + s_ToString(aln_num) + ": ";
                (*errors) += "different " + value_holder.GetFieldName() + ", ";
                (*errors) += s_ToString(value_holder.GetReference());
                (*errors) += " vs. ";
                (*errors) += s_ToString(value_holder.GetTest()) + "\n";
            }
            return false;
        }
    }
    return true;
}

bool
CSeqAlignCmp::x_MeetsEvalueRequirements(double reference, double test)
{
    if (reference != kInvalidDoubleValue && test != kInvalidDoubleValue &&
        ((reference < m_Opts.GetMaxEvalue() && test < m_Opts.GetMaxEvalue()) ||
        (reference > m_Opts.GetMinEvalue() && test > m_Opts.GetMinEvalue()))) {
        return true;
    } else {
        return false;
    }
}


/** Returns a pair containing the length of the aligned region in the query and
 * the length of the aligned region in the subject
 * @param starts starting offsets vector. Even entries represent query offsets,
 * odd entries represent subject offsets. [in] 
 * @param lengths represents the lengths of the aligned regions in the query
 * and subject. [in] 
 */
static pair<int, int>
s_GetAlignmentLengths(const vector<int>& starts,
                      const vector<TSeqPos>& lengths)
{
    int query_length = 0;
    int subject_length = 0;

    _ASSERT(lengths.size()*SeqAlign::kNumDimensions == starts.size());

    for (vector<TSeqPos>::size_type i = 0; i < lengths.size(); i++) {
        if (starts[SeqAlign::kNumDimensions*i] > 0) {
            query_length += lengths[i];
        }
        if (starts[SeqAlign::kNumDimensions*i+1] > 0) {
            subject_length += lengths[i];
        }
    }

    return make_pair(query_length, subject_length);
}

int
CSeqAlignCmp::x_CompareOneAlign(const SeqAlign* ref,
                                const SeqAlign* test,
                                int index,
                                string* errors,
                                bool allow_fuzziness)
{
    int retval = 0;

    if ( !x_MeetsEvalueRequirements(ref->evalue, test->evalue) ) {
        return retval;
    }

    // Compare evalues
    {
        string field("evalue");
        double max_diff = allow_fuzziness ? m_Opts.GetMaxEvalueDiff() : 0.0;
        CDoubleValueHolder vh(field, ref->evalue, test->evalue, max_diff);
        if ( !s_CompareValues(vh, index, errors) ) {
            retval++;
        }
    }

    // Compare bit scores
    {
        string field("bit score");
        double max_diff = allow_fuzziness ? m_Opts.GetMaxEvalueDiff() : 0.0;
        CDoubleValueHolder vh(field, ref->bit_score, test->bit_score,
                              max_diff);
        if ( !s_CompareValues(vh, index, errors) ) {
            retval++;
        }
    }

    // Compare raw scores (no fuzziness allowed)
    {
        string field("raw score");
        CIntValueHolder vh(field, ref->score, test->score);
        if ( !s_CompareValues(vh, index, errors) ) {
            retval++;
        }
    }

    // Compare number of identities (no fuzziness allowed)
    {
        string field("num identities");
        CIntValueHolder vh(field, ref->num_ident, test->num_ident);
        if ( !s_CompareValues(vh, index, errors) ) {
            retval++;
        }
    }

    // Compare number of segments (no fuzziness allowed)
    {
        string field("number of segments");
        CIntValueHolder vh(field, ref->GetNumSegments(),
                           test->GetNumSegments());
        if ( !s_CompareValues(vh, index, errors) ) {
            retval++;
        }
    }

    /* When comparing the alignment, do not go segment by segment, instead
     * compute the total length, start offset, and strand */

    // Compare start of query in aligment
    {
        string field("total query align start");
        int max_diff = allow_fuzziness ? m_Opts.GetMaxOffsetDiff() : 0;
        CIntValueHolder vh(field, ref->starts[0], test->starts[0], max_diff);
        if ( !s_CompareValues(vh, index, errors) ) {
            retval++;
        }
    }

    // Compare start of subject in aligment
    {
        string field("total subject align start");
        int max_diff = allow_fuzziness ? m_Opts.GetMaxOffsetDiff() : 0;
        CIntValueHolder vh(field, ref->starts[1], test->starts[1], max_diff);
        if ( !s_CompareValues(vh, index, errors) ) {
            retval++;
        }
    }

    pair<int, int> ref_lengths = 
        s_GetAlignmentLengths(ref->starts, ref->lengths);
    pair<int, int> test_lengths = 
        s_GetAlignmentLengths(test->starts, test->lengths);

    // Compare the length of aligned region in the query 
    {
        string field("total query align length");
        int max_diff = allow_fuzziness ? m_Opts.GetMaxLengthDiff() : 0;
        CIntValueHolder vh(field, ref_lengths.first, test_lengths.first,
                           max_diff);
        if ( !s_CompareValues(vh, index, errors) ) {
            retval++;
        }
    }

    // Compare the length of the aligned region in the subject
    {
        string field("total subject align length");
        int max_diff = allow_fuzziness ? m_Opts.GetMaxLengthDiff() : 0;
        CIntValueHolder vh(field, ref_lengths.second, test_lengths.second,
                           max_diff);
        if ( !s_CompareValues(vh, index, errors) ) {
            retval++;
        }
    }

    // Compare strand of query in aligment (no fuzziness allowed)
    {
        string field("query strand");
        CIntValueHolder vh(field, ref->query_strand, test->query_strand);
        if ( !s_CompareValues(vh, index, errors) ) {
            retval++;
        }
    }

    // Compare strand of subject in aligment (no fuzziness allowed)
    {
        string field("subject strand");
        CIntValueHolder vh(field, ref->subject_strand, test->subject_strand);
        if ( !s_CompareValues(vh, index, errors) ) {
            retval++;
        }
    }

    return retval;
}

END_SCOPE(qa)
END_SCOPE(blast)
END_SCOPE(ncbi)

