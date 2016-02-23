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
 * Authors:  Mike DiCuccio, Wratko Hlavina, Eyal Mozes
 *
 * File Description:
 *   Sample for the command-line arguments' processing ("ncbiargs.[ch]pp"):
 *
 */

#include <ncbi_pch.hpp>

#include <util/xregexp/regexp.hpp>
#include <util/range_coll.hpp>
#include <objmgr/feat_ci.hpp>
#include <objects/seqalign/Dense_seg.hpp>
#include <objects/seqalign/Spliced_seg.hpp>
#include <objects/seqalign/Spliced_exon.hpp>
#include <objects/seqalign/Product_pos.hpp>
#include <objects/seqalign/Prot_pos.hpp>

#include <objmgr/seqdesc_ci.hpp>
#include <objects/seqfeat/BioSource.hpp>
#include <objects/seqfeat/SubSource.hpp>
#include <objects/seq/MolInfo.hpp>
#include <objects/seq/Delta_ext.hpp>
#include <objects/seq/Delta_seq.hpp>
#include <objects/general/Object_id.hpp>
#include <objects/general/User_object.hpp>
#include <objects/general/User_field.hpp>

#include <objtools/alnmgr/alnmix.hpp>

#include <algo/align/util/algo_align_util_exceptions.hpp>
#include <algo/align/util/tabular_fmt.hpp>

#include <limits>

BEGIN_NCBI_SCOPE
USING_SCOPE(objects);


/////////////////////////////////////////////////////////////////////////////

CTabularFormatter_AllSeqIds::CTabularFormatter_AllSeqIds(int row)
: m_Row(row)
{
}

void CTabularFormatter_AllSeqIds::PrintHeader(CNcbiOstream& ostr) const
{
    if (m_Row == 0) {
        ostr << "query";
    } else if (m_Row == 1) {
        ostr << "subject";
    } else {
        NCBI_THROW(CException, eUnknown,
                   "only pairwise alignments are supported");
    }
}

void CTabularFormatter_AllSeqIds::PrintHelpText(CNcbiOstream& ostr) const
{
    ostr << "All ";
    PrintHeader(ostr);
    ostr << " Seq-id(s), separated by a ';'";
}

void CTabularFormatter_AllSeqIds::Print(CNcbiOstream& ostr,
                                        const CSeq_align& align)
{
    CSeq_id_Handle idh =
        CSeq_id_Handle::GetHandle(align.GetSeq_id(m_Row));
    CScope::TIds ids = m_Scores->GetScope().GetIds(idh);
    ITERATE (CScope::TIds, it, ids) {
        ostr << *it;
        CScope::TIds::const_iterator i = it;
        ++i;
        if (i != ids.end()) {
            ostr << ';';
        }
    }
}

/////////////////////////////////////////////////////////////////////////////

CTabularFormatter_SeqId::CTabularFormatter_SeqId(int row,
                                                 sequence::EGetIdType id_type)
: m_Row(row)
    , m_GetIdType(id_type)
{
}

void CTabularFormatter_SeqId::PrintHelpText(CNcbiOstream& ostr) const
{
    PrintHeader(ostr);
    switch (m_GetIdType) {
    case sequence::eGetId_Best:
        ostr << " accession.version";
        break;

    case sequence::eGetId_ForceGi:
        ostr << " GI";
        break;

    case sequence::eGetId_HandleDefault:
        ostr << " id as it appears in alignment";
        break;

    default:
        NCBI_THROW(CException, eUnknown, "Unimplemented seq-id type");
    }
}

void CTabularFormatter_SeqId::PrintHeader(CNcbiOstream& ostr) const
{
    if (m_Row == 0) {
        ostr << "query";
    } else if (m_Row == 1) {
        ostr << "subject";
    } else {
        NCBI_THROW(CException, eUnknown,
                   "only pairwise alignments are supported");
    }
}

void CTabularFormatter_SeqId::Print(CNcbiOstream& ostr,
                                    const CSeq_align& align)
{
    CSeq_id_Handle idh =
        CSeq_id_Handle::GetHandle(align.GetSeq_id(m_Row));
    CSeq_id_Handle best =
        sequence::GetId(idh, m_Scores->GetScope(), m_GetIdType);
    if ( !best ) {
        best = idh;
    }
    if (m_GetIdType == sequence::eGetId_Best) {
        string acc;
        best.GetSeqId()->GetLabel(&acc, CSeq_id::eContent);
        ostr << acc;
    } else {
        ostr << best;
    }
}

/////////////////////////////////////////////////////////////////////////////

CTabularFormatter_AlignStart::CTabularFormatter_AlignStart(int row, bool nominus)
: m_Row(row), m_NoMinus(nominus)
{
}

void CTabularFormatter_AlignStart::PrintHelpText(CNcbiOstream& ostr) const
{
    ostr << "Start of alignment in ";
    if (m_Row == 0) {
        ostr << "query";
    } else if (m_Row == 1) {
        ostr << "subject";
    } else {
        NCBI_THROW(CException, eUnknown,
                   "only pairwise alignments are supported");
    }
}

void CTabularFormatter_AlignStart::PrintHeader(CNcbiOstream& ostr) const
{
    if (m_Row == 0) {
        ostr << "qstart";
    } else if (m_Row == 1) {
        ostr << "sstart";
    } else {
        NCBI_THROW(CException, eUnknown,
                   "only pairwise alignments are supported");
    }
}

void CTabularFormatter_AlignStart::Print(CNcbiOstream& ostr,
                                         const CSeq_align& align)
{
    // determine global flip status
    
    if(m_NoMinus) {
        ostr << align.GetSeqStart(m_Row) + 1;
        return;
    }

    if (m_Row == 0) {
        TSeqRange r = align.GetSeqRange(m_Row);
        ostr << min(r.GetFrom(), r.GetTo()) + 1;
    } else {
        TSeqPos start = align.GetSeqStart(m_Row);
        TSeqPos stop  = align.GetSeqStop(m_Row);

        bool qneg  = (align.GetSeqStrand(0) == eNa_strand_minus);
        bool sneg = (align.GetSeqStrand(1) == eNa_strand_minus);

        if (qneg) {
            sneg = !sneg;
        }
        if (sneg) {
            std::swap(start, stop);
        }

        ostr << start + 1;
    }
}


/////////////////////////////////////////////////////////////////////////////

CTabularFormatter_AlignEnd::CTabularFormatter_AlignEnd(int row, bool nominus)
: m_Row(row), m_NoMinus(nominus)
{
}

void CTabularFormatter_AlignEnd::PrintHelpText(CNcbiOstream& ostr) const
{
    ostr << "End of alignment in ";
    if (m_Row == 0) {
        ostr << "query";
    } else if (m_Row == 1) {
        ostr << "subject";
    } else {
        NCBI_THROW(CException, eUnknown,
                   "only pairwise alignments are supported");
    }
}

void CTabularFormatter_AlignEnd::PrintHeader(CNcbiOstream& ostr) const
{
    if (m_Row == 0) {
        ostr << "qend";
    } else if (m_Row == 1) {
        ostr << "send";
    } else {
        NCBI_THROW(CException, eUnknown,
                   "only pairwise alignments are supported");
    }
}

void CTabularFormatter_AlignEnd::Print(CNcbiOstream& ostr,
                                       const CSeq_align& align)
{
    if(m_NoMinus) {
        ostr << align.GetSeqStop(m_Row) + 1;
        return;
    }

    if (m_Row == 0) {
        TSeqRange r = align.GetSeqRange(m_Row);
        ostr << max(r.GetFrom(), r.GetTo()) + 1;
    } else {
        TSeqPos start = align.GetSeqStart(m_Row);
        TSeqPos stop  = align.GetSeqStop(m_Row);

        bool qneg  = (align.GetSeqStrand(0) == eNa_strand_minus);
        bool sneg = (align.GetSeqStrand(1) == eNa_strand_minus);

        if (qneg) {
            sneg = !sneg;
        }
        if (sneg) {
            std::swap(start, stop);
        }

        ostr << stop + 1;
    }
}

/////////////////////////////////////////////////////////////////////////////

CTabularFormatter_AlignStrand::CTabularFormatter_AlignStrand(int row)
: m_Row(row)
{
}

void CTabularFormatter_AlignStrand::PrintHelpText(CNcbiOstream& ostr) const
{
    ostr << "Strand of alignment in ";
    if (m_Row == 0) {
        ostr << "query";
    } else if (m_Row == 1) {
        ostr << "subject";
    } else {
        NCBI_THROW(CException, eUnknown,
                   "only pairwise alignments are supported");
    }
}

void CTabularFormatter_AlignStrand::PrintHeader(CNcbiOstream& ostr) const
{
    if (m_Row == 0) {
        ostr << "qstrand";
    } else if (m_Row == 1) {
        ostr << "sstrand";
    } else {
        NCBI_THROW(CException, eUnknown,
                   "only pairwise alignments are supported");
    }
}

void CTabularFormatter_AlignStrand::Print(CNcbiOstream& ostr,
                                          const CSeq_align& align)
{
    switch (align.GetSeqStrand(m_Row)) {
    case eNa_strand_plus:
        ostr << '+';
        break;

    case eNa_strand_minus:
        ostr << '-';
        break;

    case eNa_strand_both:
        ostr << 'b';
        break;

    default:
        ostr << '?';
        break;
    }
}

/////////////////////////////////////////////////////////////////////////////

CTabularFormatter_SeqLength::CTabularFormatter_SeqLength(int row)
: m_Row(row)
{
}

void CTabularFormatter_SeqLength::PrintHelpText(CNcbiOstream& ostr) const
{
    ostr << "Length of ";
    if (m_Row == 0) {
        ostr << "query";
    } else if (m_Row == 1) {
        ostr << "subject";
    } else {
        NCBI_THROW(CException, eUnknown,
                   "only pairwise alignments are supported");
    }
    ostr << " sequence";
}

void CTabularFormatter_SeqLength::PrintHeader(CNcbiOstream& ostr) const
{
    if (m_Row == 0) {
        ostr << "qlen";
    } else if (m_Row == 1) {
        ostr << "slen";
    } else {
        NCBI_THROW(CException, eUnknown,
                   "only pairwise alignments are supported");
    }
}

void CTabularFormatter_SeqLength::Print(CNcbiOstream& ostr,
                                        const CSeq_align& align)
{
    double score =
        m_Scores->GetScore(align,
                           m_Row == 0 ? "query_length" : "subject_length");
    if (score == numeric_limits<double>::quiet_NaN()) {
        score = 0;
    }
    ostr << (int) score;
}


/////////////////////////////////////////////////////////////////////////////

void CTabularFormatter_AlignLength::PrintHelpText(CNcbiOstream& ostr) const
{
    ostr << "Alignment length";
}

void CTabularFormatter_AlignLength::PrintHeader(CNcbiOstream& ostr) const
{
    ostr << "length";
}

void CTabularFormatter_AlignLength::Print(CNcbiOstream& ostr,
                                          const CSeq_align& align)
{
    ostr << (int)m_Scores->GetScore(align, "align_length");
}

/////////////////////////////////////////////////////////////////////////////

void CTabularFormatter_AlignLengthUngap::PrintHelpText(CNcbiOstream& ostr) const
{
    ostr << "Alignment length not counting gaps";
}

void CTabularFormatter_AlignLengthUngap::PrintHeader(CNcbiOstream& ostr) const
{
    ostr << "length_ungap";
}

void CTabularFormatter_AlignLengthUngap::Print(CNcbiOstream& ostr,
                                          const CSeq_align& align)
{
    ostr << (int)m_Scores->GetScore(align, "align_length_ungap");
}

//////////////////////////////////////////////////////////////////////////////

CTabularFormatter_PercentId::CTabularFormatter_PercentId(bool gapped)
: m_Gapped(gapped)
{
}

void CTabularFormatter_PercentId::PrintHelpText(CNcbiOstream& ostr) const
{
    ostr << "Percentage of identical matches";
    if (!m_Gapped) {
        ostr << " excluding gaps";
    }
}

void CTabularFormatter_PercentId::PrintHeader(CNcbiOstream& ostr) const
{
    ostr << "pident";
    if (m_Gapped) {
        ostr << "(gapped)";
    } else {
        ostr << "(ungapped)";
    }
}

void CTabularFormatter_PercentId::Print(CNcbiOstream& ostr,
                                        const CSeq_align& align)
{
    double pct_id = m_Scores->GetScore(align,
                                       m_Gapped ? "pct_identity_gap"
                                                : "pct_identity_ungap");
    if (pct_id != 100) {
        pct_id = min(pct_id, 99.99);
    }
    ostr << pct_id;
}

/////////////////////////////////////////////////////////////////////////////

void CTabularFormatter_PercentCoverage::PrintHelpText(CNcbiOstream& ostr) const
{
    ostr << "Percent coverage of query in subject";
}

void CTabularFormatter_PercentCoverage::PrintHeader(CNcbiOstream& ostr) const
{
    ostr << "pcov";
}

void CTabularFormatter_PercentCoverage::Print(CNcbiOstream& ostr,
                                              const CSeq_align& align)
{
    double pct_cov = m_Scores->GetScore(align, "pct_coverage");
    if (pct_cov != 100) {
        pct_cov = min(pct_cov, 99.99);
    }
    ostr << pct_cov;
}

/////////////////////////////////////////////////////////////////////////////

void CTabularFormatter_GapCount::PrintHelpText(CNcbiOstream& ostr) const
{
    ostr << "Number of gap openings";
}

void CTabularFormatter_GapCount::PrintHeader(CNcbiOstream& ostr) const
{
    ostr << "gapopen";
}
void CTabularFormatter_GapCount::Print(CNcbiOstream& ostr,
                                       const CSeq_align& align)
{
    ostr << align.GetNumGapOpenings();
}

/////////////////////////////////////////////////////////////////////////////

void CTabularFormatter_IdentityCount::PrintHelpText(CNcbiOstream& ostr) const
{
    ostr << "Number of identical matches";
}
void CTabularFormatter_IdentityCount::PrintHeader(CNcbiOstream& ostr) const
{
    ostr << "identities";
}
void CTabularFormatter_IdentityCount::Print(CNcbiOstream& ostr,
                                            const CSeq_align& align)
{
    ostr << (int)m_Scores->GetScore(align, "num_ident");
}

/////////////////////////////////////////////////////////////////////////////

void CTabularFormatter_MismatchCount::PrintHelpText(CNcbiOstream& ostr) const
{
    ostr << "Number of mismatches";
}
void CTabularFormatter_MismatchCount::PrintHeader(CNcbiOstream& ostr) const
{
    ostr << "mismatch";
}
void CTabularFormatter_MismatchCount::Print(CNcbiOstream& ostr,
                                            const CSeq_align& align)
{
    ostr << (int)m_Scores->GetScore(align, "num_mismatch");
}

/////////////////////////////////////////////////////////////////////////////

void s_AlignToSeqRanges(const CSeq_align& align, int row, list<TSeqRange>& ranges)
{
    // this should be added to CSeq_align as a list instead of RangeColl version of  GetAlignedBases
    switch (align.GetSegs().Which()) {
    case CSeq_align::TSegs::e_Denseg:
        {{
            const CDense_seg& ds = align.GetSegs().GetDenseg();
            for (CDense_seg::TNumseg i = 0;  i < ds.GetNumseg();  ++i) {
                bool is_gapped = false;
                for (CDense_seg::TDim j = 0;  j < ds.GetDim();  ++j) {
                    if (ds.GetStarts()[i * ds.GetDim() + j] == -1)
                    {
                        is_gapped = true;
                        break;
                    }
                }
                if (!is_gapped) {
                    TSignedSeqPos start = ds.GetStarts()[i * ds.GetDim() + row];
                    TSeqRange range;
                    range.SetFrom(start);
                    range.SetLength(ds.GetLens()[i]);
                    ranges.push_back(range);
                }
            }
        }}
        break;
    case CSeq_align::TSegs::e_Disc:
        {{
            ITERATE(CSeq_align::TSegs::TDisc::Tdata, iter, 
                    align.GetSegs().GetDisc().Get()) {
                s_AlignToSeqRanges(*(*iter), row, ranges);
            }
        }}
        break;
    case CSeq_align::TSegs::e_Spliced:
        {{
            ITERATE (CSpliced_seg::TExons, iter, align.GetSegs().GetSpliced().GetExons()) {
                ranges.push_back((*iter)->GetRowSeq_range(row, true));
                // should this be fixed?
                //ranges -= (*iter)->GetRowSeq_insertions(row, align.GetSegs().GetSpliced());
            }
        }}
        break;
     default:
        NCBI_THROW(CSeqalignException, eUnsupported,
                   "CSeq_align::GetAlignedBases() currently does not handle "
                    "this type of alignment.");
    }
}

CTabularFormatter_MismatchPositions::CTabularFormatter_MismatchPositions(int row)
    : m_Row(row)
{
}

void CTabularFormatter_MismatchPositions::PrintHelpText(CNcbiOstream& ostr) const
{
    ostr << "Positions of aligned mismatches, comma seperated";
}
void CTabularFormatter_MismatchPositions::PrintHeader(CNcbiOstream& ostr) const
{
    if(m_Row == 0) {
        ostr << "qmismatchpos";
    } else if(m_Row == 1) {
        ostr << "smismatchpos";
    } else {
        NCBI_THROW(CException, eUnknown,
                   "only pairwise alignments are supported");
    }
}
void CTabularFormatter_MismatchPositions::Print(CNcbiOstream& ostr,
                                            const CSeq_align& align)
{
    ENa_strand QStrand = align.GetSeqStrand(0);
    ENa_strand SStrand = align.GetSeqStrand(1);

    TSeqRange QAlignRange, SAlignRange;
    QAlignRange = align.GetSeqRange(0);
    SAlignRange = align.GetSeqRange(1);

    string QueryStr, SubjtStr;
    {{
        CBioseq_Handle QueryH, SubjtH;
        QueryH = m_Scores->GetScope().GetBioseqHandle(align.GetSeq_id(0));
        SubjtH = m_Scores->GetScope().GetBioseqHandle(align.GetSeq_id(1));

        CSeqVector QueryVec(QueryH, CBioseq_Handle::eCoding_Iupac, QStrand);
 	    CSeqVector SubjtVec(SubjtH, CBioseq_Handle::eCoding_Iupac, SStrand);

        if(QStrand == eNa_strand_plus)
            QueryVec.GetSeqData(QAlignRange.GetFrom(), QAlignRange.GetTo()+1, QueryStr);
        else if(QStrand == eNa_strand_minus) 
            QueryVec.GetSeqData(QueryVec.size()-QAlignRange.GetTo()-1, 
                                QueryVec.size()-QAlignRange.GetFrom(), QueryStr);

        if(SStrand == eNa_strand_plus)
            SubjtVec.GetSeqData(SAlignRange.GetFrom(), SAlignRange.GetTo()+1, SubjtStr);
        else if(SStrand == eNa_strand_minus)
            SubjtVec.GetSeqData(SubjtVec.size()-SAlignRange.GetTo()-1, 
                                SubjtVec.size()-SAlignRange.GetFrom(), SubjtStr);

        string QS = QueryStr.substr(0,50);
        string SS = SubjtStr.substr(0,50);
    }}

    list<TSeqRange> QSegRanges, SSegRanges;
    s_AlignToSeqRanges(align, 0, QSegRanges);
    s_AlignToSeqRanges(align, 1, SSegRanges);
    

    // loop segments
    vector<TSeqPos> mm_pos;
    list<TSeqRange>::const_iterator SSegIter = SSegRanges.begin();
    ITERATE(list<TSeqRange>, QSegIter, QSegRanges) {
        TSeqRange QuerySeg = *QSegIter;
        TSeqRange SubjtSeg = *SSegIter;
        
        if(QuerySeg.GetLength() != SubjtSeg.GetLength()) {
            NCBI_THROW(CException, eUnknown, "mismatched segment sizes?");
        }

        size_t QOffset, SOffset;
        {{
            size_t QPOffset = QuerySeg.GetFrom()-QAlignRange.GetFrom();
            size_t QMOffset = QAlignRange.GetTo()-QuerySeg.GetTo();
            QOffset = (QStrand == eNa_strand_plus ? QPOffset : QMOffset);
        
            size_t SPOffset = SubjtSeg.GetFrom()-SAlignRange.GetFrom();
            size_t SMOffset = SAlignRange.GetTo()-SubjtSeg.GetTo();
            SOffset = (SStrand == eNa_strand_plus ? SPOffset : SMOffset);
        }}

        // find locations
        for(size_t Loop = 0; Loop < QuerySeg.GetLength(); Loop++) {
            size_t QLoop = QOffset+Loop;
            size_t SLoop = SOffset+Loop;
          
            //if(Loop < 6 || Loop+6 > QuerySeg.GetLength())
            //    cerr << "L: " << Loop << " "  << QLoop << " " << SLoop << " : " 
            //    << QueryStr[QLoop] << " == " << SubjtStr[SLoop] << endl;
            

            if(QueryStr[QLoop] == SubjtStr[SLoop]) {
                ; 
            } else {
                if(m_Row == 0)
                    mm_pos.push_back(QuerySeg.GetFrom()+Loop);
                else if(m_Row == 0)
                    mm_pos.push_back(SubjtSeg.GetFrom()+Loop);
            }
        }

        ++SSegIter;
    }
   
    sort(mm_pos.begin(), mm_pos.end());
    ITERATE(vector<TSeqPos>, it, mm_pos) {
        if (it != mm_pos.begin()) {
            ostr << ',';
        }
        ostr << *it +1; 
    }
}

/////////////////////////////////////////////////////////////////////////////

CTabularFormatter_GapRanges::CTabularFormatter_GapRanges(int row)
    : m_Row(row)
{
}

void CTabularFormatter_GapRanges::PrintHelpText(CNcbiOstream& ostr) const
{
    ostr << "Positions of gapped, unaligned, segments, comma seperated";
}
void CTabularFormatter_GapRanges::PrintHeader(CNcbiOstream& ostr) const
{
    if(m_Row == 0) {
        ostr << "qgapranges";
    } else if(m_Row == 1) {
        ostr << "sgapranges";
    } else {
        NCBI_THROW(CException, eUnknown,
                   "only pairwise alignments are supported");
    }
}
void CTabularFormatter_GapRanges::Print(CNcbiOstream& ostr,
                                        const CSeq_align& align)
{
    CRangeCollection<TSeqPos> AlignedRC = align.GetAlignedBases(m_Row);
    CRangeCollection<TSeqPos> GappedRC;
    GappedRC += align.GetSeqRange(m_Row);
    GappedRC -= AlignedRC;

    //vector<TSeqPos> mm_pos;
    //sort(mm_pos.begin(), mm_pos.end());
    ITERATE(CRangeCollection<TSeqPos>, it, GappedRC) {
        if (it != GappedRC.begin()) {
            ostr << ',';
        }
        ostr << it->GetFrom()+1 << "-" << it->GetTo()+1; 
    }
}

/////////////////////////////////////////////////////////////////////////////

void CTabularFormatter_GapBaseCount::PrintHelpText(CNcbiOstream& ostr) const
{
    ostr << "Total number of gaps";
}
void CTabularFormatter_GapBaseCount::PrintHeader(CNcbiOstream& ostr) const
{
    ostr << "gaps";
}
void CTabularFormatter_GapBaseCount::Print(CNcbiOstream& ostr,
                                           const CSeq_align& align)
{
    ostr << align.GetTotalGapCount();
}

/////////////////////////////////////////////////////////////////////////////

void CTabularFormatter_EValue::PrintHelpText(CNcbiOstream& ostr) const
{
    ostr << "Expect value";
}
void CTabularFormatter_EValue::PrintHeader(CNcbiOstream& ostr) const
{
    ostr << "evalue";
}
void CTabularFormatter_EValue::Print(CNcbiOstream& ostr,
                                     const CSeq_align& align)
{
    double score = m_Scores->GetScore(align, "e_value");
    if (score == numeric_limits<double>::infinity()  ||
        score == numeric_limits<double>::quiet_NaN()) {
        score = 0;
    }
    if (score > 1e26) {
        score = 0;
    }
    if (score <  -1e26) {
        score = 0;
    }

    //get the current flags
    ios_base::fmtflags cur_flags=ostr.flags();

    //print using scientific
    ostr << scientific << score;

    //unset scientific
    ostr.unsetf(ios_base::scientific);

    //reset to original flags
    ostr << setiosflags(cur_flags);
}


/////////////////////////////////////////////////////////////////////////////

void CTabularFormatter_EValue_Mantissa::PrintHelpText(CNcbiOstream& ostr) const
{
    ostr << "Expect value in mantissa format";
}
void CTabularFormatter_EValue_Mantissa::PrintHeader(CNcbiOstream& ostr) const
{
    ostr << "evalue_mantissa";
}
void CTabularFormatter_EValue_Mantissa::Print(CNcbiOstream& ostr,
                                              const CSeq_align& align)
{
    double score = 0;
    if ( !align.GetNamedScore(CSeq_align::eScore_EValue, score) ) {
        score = m_Scores->GetScore(align, "e_value");
    }
    if (score == numeric_limits<double>::infinity()  ||
        score == numeric_limits<double>::quiet_NaN()) {
        score = 0;
    }
    if (score > 1e26) {
        score = 0;
    }
    if (score <  -1e26) {
        score = 0;
    }

    double mantissa = score;
    int exponent = 0;

    if(score > 0.0) {
        while(mantissa >= 10.0) {
            mantissa /= 10.0;
            exponent++;
        }
        while(mantissa < 1.0) {
            mantissa *= 10.0;
            exponent--;
        }
    } else if(score < 0.0) {
        while(mantissa <= -10.0) {
            mantissa /= 10.0;
            exponent--;
        }
        while(mantissa > -1.0) {
            mantissa *= 10.0;
            exponent++;
        }
    }
    
    ostr << mantissa;
}


/////////////////////////////////////////////////////////////////////////////

void CTabularFormatter_EValue_Exponent::PrintHelpText(CNcbiOstream& ostr) const
{
    ostr << "Expect value in exponent format";
}
void CTabularFormatter_EValue_Exponent::PrintHeader(CNcbiOstream& ostr) const
{
    ostr << "evalue_exponent";
}
void CTabularFormatter_EValue_Exponent::Print(CNcbiOstream& ostr,
                                     const CSeq_align& align)
{
    double score = 0;
    if ( !align.GetNamedScore(CSeq_align::eScore_EValue, score) ) {
        score = m_Scores->GetScore(align, "e_value");
    }
    if (score == numeric_limits<double>::infinity()  ||
        score == numeric_limits<double>::quiet_NaN()) {
        score = 0;
    }
    if (score > 1e26) {
        score = 0;
    }
    if (score <  -1e26) {
        score = 0;
    }

    double mantissa = score;
    int exponent = 0;


    if(score > 0.0) {
        while(mantissa >= 10.0) {
            mantissa /= 10.0;
            exponent++;
        }
        while(mantissa < 1.0) {
            mantissa *= 10.0;
            exponent--;
        }
    } else if(score < 0.0) {
        while(mantissa <= -10.0) {
            mantissa /= 10.0;
            exponent--;
        }
        while(mantissa > -1.0) {
            mantissa *= 10.0;
            exponent++;
        }
    }
    
    ostr << exponent;
}





/////////////////////////////////////////////////////////////////////////////

void CTabularFormatter_BitScore::PrintHelpText(CNcbiOstream& ostr) const
{
    ostr << "Bit score";
}
void CTabularFormatter_BitScore::PrintHeader(CNcbiOstream& ostr) const
{
    ostr << "bitscore";
}
void CTabularFormatter_BitScore::Print(CNcbiOstream& ostr,
                                       const CSeq_align& align)
{
    double score = m_Scores->GetScore(align, "bit_score");
    ostr << score;
}

/////////////////////////////////////////////////////////////////////////////

void CTabularFormatter_Score::PrintHelpText(CNcbiOstream& ostr) const
{
    ostr << "Raw score";
}
void CTabularFormatter_Score::PrintHeader(CNcbiOstream& ostr) const
{
    ostr << "score";
}
void CTabularFormatter_Score::Print(CNcbiOstream& ostr,
                                    const CSeq_align& align)
{
    double score = m_Scores->GetScore(align, "score");
    ostr << score;
}

/////////////////////////////////////////////////////////////////////////////

/// formatter for dumping any score in an alignment
CTabularFormatter_AnyScore::CTabularFormatter_AnyScore(const string& score_name,
                           const string& col_name)
    : m_ScoreName(score_name)
    , m_ColName(col_name)
{
}


void CTabularFormatter_AnyScore::PrintHelpText(CNcbiOstream& ostr) const
{
    ostr << m_Scores->HelpText(m_ScoreName);
}

void CTabularFormatter_AnyScore::PrintHeader(CNcbiOstream& ostr) const
{
    ostr << m_ScoreName;
}


void CTabularFormatter_AnyScore::Print(CNcbiOstream& ostr,
                                       const CSeq_align& align)
{
    double score_d=0.0;
    int score_i = 0;
    bool is_int = m_Scores->IsIntegerScore(align, m_ScoreName);
    try {
        if(is_int)
            score_i = (int)m_Scores->GetScore(align, m_ScoreName);  
        else
            score_d = m_Scores->GetScore(align, m_ScoreName);
    } catch (CAlgoAlignUtilException &) {
        score_d = 0;
        score_i = 0;
    }
    if(is_int)
        ostr << score_i;
    else
        ostr << score_d;
}


/////////////////////////////////////////////////////////////////////////////

CTabularFormatter_Defline::CTabularFormatter_Defline(int row)
    : m_Row(row)
{
}


void CTabularFormatter_Defline::PrintHelpText(CNcbiOstream& ostr) const
{
    ostr << "Defline of the ";
    if (m_Row == 0) {
        ostr << "query";
    } else if (m_Row == 1) {
        ostr << "subject";
    } else {
        NCBI_THROW(CException, eUnknown,
                   "only pairwise alignments are supported");
    }
    ostr << " sequence";
}


void CTabularFormatter_Defline::PrintHeader(CNcbiOstream& ostr) const
{
    if (m_Row == 0) {
        ostr << "qdefline";
    } else if (m_Row == 1) {
        ostr << "sdefline";
    } else {
        NCBI_THROW(CException, eUnknown,
                   "only pairwise alignments are supported");
    }
}


void CTabularFormatter_Defline::Print(CNcbiOstream& ostr,
                                      const CSeq_align& align)
{
    if (m_Row >= align.CheckNumRows()) {
        NCBI_THROW(CException, eUnknown,
                   "indexing past the end of available "
                   "sequences in an alignment");
    }

    CSeq_id_Handle idh = CSeq_id_Handle::GetHandle(align.GetSeq_id(m_Row));
    CBioseq_Handle bsh = m_Scores->GetScope().GetBioseqHandle(idh);
    if (bsh) {
        ostr << generator.GenerateDefline(bsh);
    }
}

/////////////////////////////////////////////////////////////////////////////

void CTabularFormatter_AlignId::PrintHelpText(CNcbiOstream& ostr) const
{
    ostr << "Alignment ids";
}


void CTabularFormatter_AlignId::PrintHeader(CNcbiOstream& ostr) const
{
    ostr << "align_ids";
}


void CTabularFormatter_AlignId::Print(CNcbiOstream& ostr,
                                      const CSeq_align& align)
{
    if (align.IsSetId()) {
        bool first = true;
        ITERATE (CSeq_align::TId, it, align.GetId()) {
            if ( !first ) {
                ostr << ',';
            }
            if ((*it)->IsId()) {
                ostr << (*it)->GetId();
            }
            else if ((*it)->IsStr()) {
                ostr << (*it)->GetStr();
            }
        }
    }
}

/////////////////////////////////////////////////////////////////////////////

void CTabularFormatter_BestPlacementGroup::PrintHelpText(CNcbiOstream& ostr) const
{
    ostr << "best_placement group id";
}

void CTabularFormatter_BestPlacementGroup::PrintHeader(CNcbiOstream& ostr) const
{
    ostr << "best_placement_group";
}


void CTabularFormatter_BestPlacementGroup::Print(CNcbiOstream& ostr,
                                                 const CSeq_align& align)
{
    if (align.IsSetExt()) {
        ITERATE (CSeq_align::TExt, i, align.GetExt()) {
            const CUser_object& obj = **i;
            if (!obj.GetType().IsStr()  ||
                obj.GetType().GetStr() != "placement_data") {
                continue;
            }

            CConstRef<CUser_field> f = obj.GetFieldRef("placement_id");
            if (f) {
                ostr << f->GetData().GetStr();
                break;
            }
        }
    }
}

/////////////////////////////////////////////////////////////////////////////

CTabularFormatter_ProtRef::CTabularFormatter_ProtRef(int row)
    : m_Row(row)
{
}


void CTabularFormatter_ProtRef::PrintHelpText(CNcbiOstream& ostr) const
{
    ostr << "Prot-ref of the ";
    if (m_Row == 0) {
        ostr << "query";
    } else if (m_Row == 1) {
        ostr << "subject";
    } else {
        NCBI_THROW(CException, eUnknown,
                   "only pairwise alignments are supported");
    }
    ostr << " sequence";
}


void CTabularFormatter_ProtRef::PrintHeader(CNcbiOstream& ostr) const
{
    if (m_Row == 0) {
        ostr << "qprotref";
    } else if (m_Row == 1) {
        ostr << "sprotref";
    } else {
        NCBI_THROW(CException, eUnknown,
                   "only pairwise alignments are supported");
    }
}


void CTabularFormatter_ProtRef::Print(CNcbiOstream& ostr,
                                      const CSeq_align& align)
{
    if (m_Row >= align.CheckNumRows()) {
        NCBI_THROW(CException, eUnknown,
                   "indexing past the end of available "
                   "sequences in an alignment");
    }

    CSeq_id_Handle idh = CSeq_id_Handle::GetHandle(align.GetSeq_id(m_Row));
    CBioseq_Handle bsh = m_Scores->GetScope().GetBioseqHandle(idh);
    if (bsh) {
        SAnnotSelector sel;
        sel.SetResolveTSE()
            .IncludeFeatType(CSeqFeatData::e_Prot);
        CFeat_CI feat_iter(bsh, sel);
        if (feat_iter.GetSize() == 1) {
            const CProt_ref& ref = feat_iter->GetData().GetProt();
            string s;
            ref.GetLabel(&s);
            ostr << s;
        }
    }
}

/////////////////////////////////////////////////////////////////////////////


void CTabularFormatter_ExonIntrons::PrintHelpText(CNcbiOstream& ostr) const
{
    ostr << "Dump the ";
    switch (m_Interval) {
    case e_Exons:
        ostr << "exon";
        break;

    case e_Introns:
        ostr << (m_Sequence == 0 ? "unaligned segment" : "intron");
        break;
    }

    switch (m_Info) {
    case e_Range:
        ostr << " structure";
        break;

    case e_Length:
        ostr << " lengths";
        break;
    }

    if (m_Sequence == 0) {
        ostr << " for the query sequence";
    }

    ostr << " of a Spliced-seg alignment";
}

void CTabularFormatter_ExonIntrons::PrintHeader(CNcbiOstream& ostr) const
{
    if (m_Sequence == 0) {
        ostr << "query_";
    }
    switch (m_Interval) {
    case e_Exons:
        switch (m_Info) {
        case e_Range:
            ostr << "exons";
            break;

        case e_Length:
            ostr << "exon_len";
            break;
        }
        break;

    case e_Introns:
        switch (m_Info) {
        case e_Range:
            ostr << (m_Sequence == 0 ? "unaligned" : "introns");
            break;

        case e_Length:
            ostr << (m_Sequence == 0 ? "unaligned_len" : "intron_len");
            break;
        }
        break;

    }
}

void CTabularFormatter_ExonIntrons::Print(CNcbiOstream& ostr,
                                    const CSeq_align& align)
{
    if (align.GetSegs().IsSpliced()) {
        bool is_protein = m_Sequence == 0 && 
                          align.GetSegs().GetSpliced().GetProduct_type() ==
                              CSpliced_seg::eProduct_type_protein;
        if (is_protein && (m_Interval == e_Introns || m_Info == e_Length)) {
            CNcbiOstrstream column_name;
            PrintHeader(column_name);
            NCBI_THROW(CException, eUnknown,
                       string(CNcbiOstrstreamToString(column_name))
                       + " not supported for protein alignments");
        }

        typedef pair<const CProt_pos*, const CProt_pos*> TProteinExon;
        vector<TProteinExon> protein_exons;
        vector<TSeqRange> nuc_exons;

        CRangeCollection<TSeqPos> intron_ranges;
        if (m_Interval == e_Introns) {
            TSeqRange align_range = align.GetSeqRange(m_Sequence);
            align_range.SetFrom(align_range.GetFrom()+1);
            align_range.SetTo(align_range.GetTo()+1);
            intron_ranges += align_range;
        }

        ITERATE (CSpliced_seg::TExons, it,
                 align.GetSegs().GetSpliced().GetExons()) {
            const CSpliced_exon& exon = **it;
            TSeqRange exon_range;
            if (is_protein) {
                protein_exons.push_back(
                    TProteinExon(
                        &exon.GetProduct_start().GetProtpos(),
                        &exon.GetProduct_end().GetProtpos()));
            } else if (m_Sequence == 1) {
                exon_range.SetFrom(exon.GetGenomic_start()+1);
                exon_range.SetTo(exon.GetGenomic_end()+1);
            } else {
                exon_range.SetFrom(exon.GetProduct_start().GetNucpos()+1);
                exon_range.SetTo(exon.GetProduct_end().GetNucpos()+1);
            }
            switch (m_Interval) {
            case e_Exons:
                nuc_exons.push_back(exon_range);
                break;

            case e_Introns:
                intron_ranges -= exon_range;
                break;
            }
        }
        list<TSeqRange> range_list;
        if (!nuc_exons.empty()) {
            range_list.insert(range_list.end(), nuc_exons.begin(),
                                                nuc_exons.end());
        } else if (!intron_ranges.Empty()) {
            range_list.insert(range_list.end(), intron_ranges.begin(),
                                                intron_ranges.end());
            if(m_Sequence == 1 &&
               (align.GetSeqStrand(0) == eNa_strand_minus ||
                align.GetSeqStrand(1) == eNa_strand_minus))
            {
                range_list.reverse();
            }
        }
        ostr << '[';
        if (is_protein) {
            ITERATE (vector<TProteinExon>, it, protein_exons) {
                if (it != protein_exons.begin()) {
                    ostr << ',';
                }

                ostr << '(' << it->first->GetAmin()+1
                     << '/' << it->first->GetFrame()
                     << ".." << it->second->GetAmin()+1
                     << '/' << it->second->GetFrame() << ')';
            }
        } else {
            ITERATE (list<TSeqRange>, it, range_list) {
                if (it != range_list.begin()) {
                    ostr << ',';
                }

                switch (m_Info) {
                case e_Range:
                    ostr << '('
                        << it->GetFrom()
                        << ".."
                        << it->GetTo()
                        << ')';
                    break;

                case e_Length:
                    ostr << it->GetLength();
                    break;
                }
            }
        }
        ostr << ']';
    }
}

/////////////////////////////////////////////////////////////////////////////

CTabularFormatter_TaxId::CTabularFormatter_TaxId(int row)
    : m_Row(row)
{
}


void CTabularFormatter_TaxId::PrintHelpText(CNcbiOstream& ostr) const
{
    ostr << "Taxid of the ";
    if (m_Row == 0) {
        ostr << "query";
    } else if (m_Row == 1) {
        ostr << "subject";
    } else {
        NCBI_THROW(CException, eUnknown,
                   "only pairwise alignments are supported");
    }
    ostr << " sequence";
}

void CTabularFormatter_TaxId::PrintHeader(CNcbiOstream& ostr) const
{
    if (m_Row == 0) {
        ostr << "qtaxid";
    } else if (m_Row == 1) {
        ostr << "staxid";
    } else {
        NCBI_THROW(CException, eUnknown,
                   "only pairwise alignments are supported");
    }
}

void CTabularFormatter_TaxId::Print(CNcbiOstream& ostr,
                                    const CSeq_align& align)
{
    if (m_Row >= align.CheckNumRows()) {
        NCBI_THROW(CException, eUnknown,
                   "indexing past the end of available "
                   "sequences in an alignment");
    }

    ostr << (int)m_Scores->GetScore(align, m_Row == 0 ? "query_taxid"
                                                 : "subject_taxid");
}


/////////////////////////////////////////////////////////////////////////////
CTabularFormatter_BiggestGapBases::CTabularFormatter_BiggestGapBases(int row)
: m_Row(row)
{
}

void CTabularFormatter_BiggestGapBases::PrintHelpText(CNcbiOstream& ostr) const
{
    ostr << "size of biggest gap";
}
void CTabularFormatter_BiggestGapBases::PrintHeader(CNcbiOstream& ostr) const
{
    if(m_Row == e_All) {
        ostr << "biggestgap";
    } else if(m_Row == 0) {
        ostr << "qbiggestgap";
    } else if(m_Row == 1) {
        ostr << "sbiggestgap";
    } else {
        NCBI_THROW(CException, eUnknown,
        "only pairwise alignments are supported");
    }
}
void CTabularFormatter_BiggestGapBases::Print(CNcbiOstream& ostr,
                                              const CSeq_align& align)
{
    ostr << x_CalcBiggestGap(align);
}

TSeqPos CTabularFormatter_BiggestGapBases::x_CalcBiggestGap(const CSeq_align& align)
{
    if(align.GetSegs().IsDisc()) {
        TSeqPos Biggest = 0;
        ITERATE(CSeq_align_set::Tdata, AlignIter, align.GetSegs().GetDisc().Get()) {
            Biggest = max(Biggest, x_CalcBiggestGap(**AlignIter));
        }
        return Biggest;
    } else if(align.GetSegs().IsDenseg()) {
        const CDense_seg& Denseg = align.GetSegs().GetDenseg();
        TSeqPos Biggest = 0;
        for(int Index = 0; Index < Denseg.GetNumseg(); Index++) {
            bool QGap = (Denseg.GetStarts()[2*Index] == -1);
            bool SGap = (Denseg.GetStarts()[(2*Index)+1] == -1);
            if(m_Row == e_All && (QGap || SGap)) { 
                Biggest = max(Biggest, (TSeqPos)Denseg.GetLens()[Index]);
            } else if(m_Row == 0 && QGap) {
                Biggest = max(Biggest, (TSeqPos)Denseg.GetLens()[Index]);
            } else if(m_Row == 1 && SGap) {
                Biggest = max(Biggest, (TSeqPos)Denseg.GetLens()[Index]);
            }
        }
        return Biggest;
    } else {
        NCBI_THROW(CException, eUnknown,
                   "biggestgap is only supported for Dense-sef and Disc alignments");
    }
}

/////////////////////////////////////////////////////////////////////////////
CTabularFormatter_SeqChrom::CTabularFormatter_SeqChrom(int row)
: m_Row(row)
{
}

void CTabularFormatter_SeqChrom::PrintHelpText(CNcbiOstream& ostr) const
{
    ostr << "If ";
    if (m_Row == 0) {
        ostr << "query";
    } else if (m_Row == 1) {
        ostr << "subject";
    } else {
        NCBI_THROW(CException, eUnknown,
        "only pairwise alignments are supported");
    }
    ostr << " has a chromosome, its name";
}

void CTabularFormatter_SeqChrom::PrintHeader(CNcbiOstream& ostr) const
{
    if (m_Row == 0) {
        ostr << "qchrom";
    } else if (m_Row == 1) {
        ostr << "schrom";
    } else {
        NCBI_THROW(CException, eUnknown,
        "only pairwise alignments are supported");
    }
}

void CTabularFormatter_SeqChrom::Print(CNcbiOstream& ostr,
                                       const CSeq_align& align)
{
    CBioseq_Handle Handle = m_Scores->GetScope().GetBioseqHandle(align.GetSeq_id(m_Row));

    string Chrom = "";

    CSeqdesc_CI Iter(Handle, CSeqdesc::e_Source);
    while(Iter) {
        const CBioSource& BioSource = Iter->GetSource();
        if(BioSource.CanGetSubtype()) {
            ITERATE(CBioSource::TSubtype, SubIter, BioSource.GetSubtype()) {
                if( (*SubIter)->CanGetSubtype() &&
                    (*SubIter)->GetSubtype() == CSubSource::eSubtype_chromosome &&
                    (*SubIter)->CanGetName() ) {
                    Chrom = (*SubIter)->GetName();
                }
            }
        }
        ++Iter;
    }

    ostr << Chrom;
}

/////////////////////////////////////////////////////////////////////////////
CTabularFormatter_SeqClone::CTabularFormatter_SeqClone(int row)
: m_Row(row)
{
}

void CTabularFormatter_SeqClone::PrintHelpText(CNcbiOstream& ostr) const
{
    ostr << "If ";
    if (m_Row == 0) {
        ostr << "query";
    } else if (m_Row == 1) {
        ostr << "subject";
    } else {
        NCBI_THROW(CException, eUnknown,
        "only pairwise alignments are supported");
    }
    ostr << " has a clone, its name";
}

void CTabularFormatter_SeqClone::PrintHeader(CNcbiOstream& ostr) const
{
    if (m_Row == 0) {
        ostr << "qclone";
    } else if (m_Row == 1) {
        ostr << "sclone";
    } else {
        NCBI_THROW(CException, eUnknown,
        "only pairwise alignments are supported");
    }
}

void CTabularFormatter_SeqClone::Print(CNcbiOstream& ostr,
                                       const CSeq_align& align)
{
    string Clone = "";
   
    try {
        CBioseq_Handle Handle = m_Scores->GetScope().GetBioseqHandle(align.GetSeq_id(m_Row));
        CSeqdesc_CI Iter(Handle, CSeqdesc::e_Source);
        while(Iter) {
            const CBioSource& BioSource = Iter->GetSource();
            if(BioSource.CanGetSubtype()) {
                ITERATE(CBioSource::TSubtype, SubIter, BioSource.GetSubtype()) {
                    if( (*SubIter)->CanGetSubtype() &&
                        (*SubIter)->GetSubtype() == CSubSource::eSubtype_clone &&
                        (*SubIter)->CanGetName() ) {
                        Clone = (*SubIter)->GetName();
                    }
                }
            }
            ++Iter;
        }
    } catch(...) {
        Clone = "";
    }

    ostr << Clone;
}


/////////////////////////////////////////////////////////////////////////////
CTabularFormatter_Tech::CTabularFormatter_Tech(int row)
: m_Row(row)
{
}

void CTabularFormatter_Tech::PrintHelpText(CNcbiOstream& ostr) const
{
    if (m_Row == 0) {
        ostr << "Query";
    } else if (m_Row == 1) {
        ostr << "Subject";
    } else {
        NCBI_THROW(CException, eUnknown,
        "only pairwise alignments are supported");
    }
    ostr << " sequence tech type";
}

void CTabularFormatter_Tech::PrintHeader(CNcbiOstream& ostr) const
{
    if (m_Row == 0) {
        ostr << "qtech";
    } else if (m_Row == 1) {
        ostr << "stech";
    } else {
        NCBI_THROW(CException, eUnknown,
        "only pairwise alignments are supported");
    }
}

void CTabularFormatter_Tech::Print(CNcbiOstream& ostr,
                                       const CSeq_align& align)
{
    CBioseq_Handle Handle = m_Scores->GetScope().GetBioseqHandle(align.GetSeq_id(m_Row));

    string TechStr = "(none)";

    CSeqdesc_CI Iter(Handle, CSeqdesc::e_Molinfo);
    while(Iter) {
        const CMolInfo& MolInfo = Iter->GetMolinfo();
        if(MolInfo.CanGetTech() && MolInfo.IsSetTech()) {
            const CEnumeratedTypeValues* tech_types = CMolInfo::GetTypeInfo_enum_ETech();
            TechStr = tech_types->FindName(MolInfo.GetTech(), false);
        }
        ++Iter;
    }

    ostr << TechStr;
}

//////////////////////////////////////////////////////////////////////////////
CTabularFormatter_DiscStrand::CTabularFormatter_DiscStrand(int row)
: m_Row(row)
{
}

void CTabularFormatter_DiscStrand::PrintHelpText(CNcbiOstream& ostr) const
{
    ostr << "Strand of alignment in ";
    if (m_Row == 0) {
        ostr << "query";
    } else if (m_Row == 1) {
        ostr << "subject";
    } else {
        NCBI_THROW(CException, eUnknown,
                   "only pairwise alignments are supported");
    }
    ostr << ", 'b' if both in a Disc-seg alignment";
}

void CTabularFormatter_DiscStrand::PrintHeader(CNcbiOstream& ostr) const
{
    if (m_Row == 0) {
        ostr << "qdiscstrand";
    } else if (m_Row == 1) {
        ostr << "sdiscstrand";
    } else {
        NCBI_THROW(CException, eUnknown,
                   "only pairwise alignments are supported");
    }
}

void CTabularFormatter_DiscStrand::Print(CNcbiOstream& ostr,
                                          const CSeq_align& align)
{
    bool Plus=false, Minus=false;
    x_RecurseStrands(align, Plus, Minus);
    if(Plus && !Minus)
        ostr << '+';
    else if(Minus && !Plus)
        ostr << '-';
    else if(Plus && Minus)
        ostr << 'b';
}

void CTabularFormatter_DiscStrand::x_RecurseStrands(const CSeq_align& align,
                                                     bool& Plus, bool& Minus)
{
    if(align.GetSegs().IsDisc()) {
        ITERATE(CSeq_align_set::Tdata, iter, align.GetSegs().GetDisc().Get()) {
            x_RecurseStrands(**iter, Plus, Minus);
        }
        return;
    }

    if(align.GetSeqStrand(m_Row) == eNa_strand_plus)
        Plus = true;
    else if(align.GetSeqStrand(m_Row) == eNa_strand_minus)
        Minus = true;
}


//////////////////////////////////////////////////////////////////////////////

CTabularFormatter_FixedText::CTabularFormatter_FixedText(const string& col_name,
                                                         const string& text)
: m_ColName(col_name)
, m_Text(text)
{
}

void CTabularFormatter_FixedText::PrintHelpText(CNcbiOstream& ostr) const
{
    ostr << "'" << m_Text << "' as fixed text";
}

void CTabularFormatter_FixedText::PrintHeader(CNcbiOstream& ostr) const
{
    ostr << m_ColName;
}

void CTabularFormatter_FixedText::Print(CNcbiOstream& ostr,
                                        const CSeq_align& align)
{
    ostr << m_Text;
}


//////////////////////////////////////////////////////////////////////////////

void CTabularFormatter_AlignLengthRatio::PrintHelpText(CNcbiOstream& ostr) const
{
    ostr << "length_ungap / size of aligned query sequence range";
}

void CTabularFormatter_AlignLengthRatio::PrintHeader(CNcbiOstream& ostr) const
{
    ostr << "align_len_ratio";
}

void CTabularFormatter_AlignLengthRatio::Print(CNcbiOstream& ostr,
                                               const CSeq_align& align)
{
    /// historical score:
    /// ungapped alignment length / length of range of query sequence
    TSeqPos align_length = align.GetAlignLength(false /*ungapped*/);
    TSeqPos align_range = align.GetSeqRange(0).GetLength();
    ostr << double(align_length) / double(align_range);
}


/////////////////////////////////////////////////////////////////////////////

CTabularFormatter_Cigar::CTabularFormatter_Cigar()
{
}


void CTabularFormatter_Cigar::PrintHelpText(CNcbiOstream& ostr) const
{
    ostr << "Alignment CIGAR string";
}


void CTabularFormatter_Cigar::PrintHeader(CNcbiOstream& ostr) const
{
    ostr << "cigar";
}


void CTabularFormatter_Cigar::Print(CNcbiOstream& ostr,
                                    const CSeq_align& align)
{
    if(!align.CanGetSegs() || !align.GetSegs().IsDenseg()) {
        NCBI_THROW(CException, eUnknown,
                    "cigar format only supports denseg alignments.");
    }


    int NumSeg = align.GetSegs().GetDenseg().GetNumseg();
    const CDense_seg::TStarts & Starts = align.GetSegs().GetDenseg().GetStarts();
    const CDense_seg::TLens & Lens = align.GetSegs().GetDenseg().GetLens();
    
    for(int Loop = 0; Loop < NumSeg; Loop++) {
        int Length = Lens[Loop];
        char Code = 0;
        
        if( Starts[ (Loop*2) ] == -1)
            Code = 'D';
        else if( Starts[ (Loop*2)+1 ] == -1)
            Code = 'I';
        else
            Code = 'M';

        if(Length == 1)
            ostr << Code;
        else
            ostr << Length << Code;
    }

}


//////////////////////////////////////////////////////////////////////////////

CTabularFormatter_AssemblyInfo::
CTabularFormatter_AssemblyInfo(int row, EAssemblyType type, EInfo info)
: m_Row(row), m_Type(type), m_Info(info)
{
}

void CTabularFormatter_AssemblyInfo::SetGencoll(CConstRef<CGC_Assembly> gencoll)
{
    m_Gencoll = gencoll;
}


void CTabularFormatter_AssemblyInfo::PrintHelpText(CNcbiOstream& ostr) const
{
    switch (m_Info) {
    case eName:
        ostr << "Name of ";
        break;
    case eAccession:
        ostr << "Accession of ";
        break;
    case eChainId:
        ostr << "Chain id of ";
        break;
    case eChromosome:
        ostr << "Chromosome containing ";
        break;
    }
    if (m_Info != eChromosome) {
        ostr << (m_Type == eFull ? "full assembly" : "assembly unit") << " of ";
    }
    if (m_Row == 0) {
        ostr << "query";
    } else if (m_Row == 1) {
        ostr << "subject";
    } else {
        NCBI_THROW(CException, eUnknown,
                   "only pairwise alignments are supported");
    }
    ostr << " sequence";
}

void CTabularFormatter_AssemblyInfo::PrintHeader(CNcbiOstream& ostr) const
{
    if (m_Row == 0) {
        ostr << "q";
    } else if (m_Row == 1) {
        ostr << "s";
    } else {
        NCBI_THROW(CException, eUnknown,
                   "only pairwise alignments are supported");
    }
    if (m_Info != eChromosome) {
        ostr << (m_Type == eFull ? "fullasm" : "asmunit");
    }
    switch (m_Info) {
    case eName:
        break;
    case eAccession:
        ostr << "acc";
        break;
    case eChainId:
        ostr << "chain";
        break;
    case eChromosome:
        ostr << "chromosome";
        break;
    }
}

void CTabularFormatter_AssemblyInfo::Print(CNcbiOstream& ostr,
                                      const CSeq_align& align)
{
    if (m_Row == 1 && align.IsSetExt())
    {
        /// For the subject sequence, the information may be stored in teh
        /// alignment as a User-obejct
        ITERATE (CSeq_align::TExt, i, align.GetExt()) {
            const CUser_object& obj = **i;
            if (!obj.GetType().IsStr()  ||
                obj.GetType().GetStr() != "Assembly Info") {
                continue;
            }

            switch (m_Info) {
            case eName:
                ostr << obj.GetField(m_Type == eFull
                            ? "Assembly Name" : "Assembly Unit Name")
                                      .GetData().GetStr();
                return;

            case eAccession:
                if (obj.HasField("Assembly Accession")) {
                    ostr << obj.GetField(m_Type == eFull
                                ? "Assembly Accession" : "Assembly Unit Accession")
                                          .GetData().GetStr();
                } else {
                    ostr << "NA";
                }
                return;

            case eChainId:
                if (m_Type == eUnit) {
                    if (obj.HasField("GenColl Chain")) {
                        ostr << obj.GetField("GenColl Chain").GetData().GetInt();
                    } else {
                        ostr << "NA";
                    }
                    return;
                }
                break;

            case eChromosome:
                if (obj.HasField("Chromosome")) {
                    ostr << obj.GetField("Chromosome").GetData().GetStr();
                } else {
                    ostr << "NA";
                }
                return;
            }
        }
    }

    if(!m_Gencoll) {
        return;
    }
    
    CConstRef<CGC_Sequence> Seq;
    CGC_Assembly::TSequenceList SeqList;
    m_Gencoll->Find(CSeq_id_Handle::GetHandle(align.GetSeq_id(m_Row)), SeqList);
    if(!SeqList.empty())
        Seq = SeqList.front();

    if(!Seq) {
        return;
    }

    CConstRef<CGC_Assembly> Assm;
    if (m_Info != eChromosome) {
        if (m_Type == eFull) {
            Assm = Seq->GetFullAssembly();
        } else {
            CConstRef<CGC_AssemblyUnit> Unit = Seq->GetAssemblyUnit();
            if (Unit) {
                CGC_Assembly *unit_assm = new CGC_Assembly();
                unit_assm->SetUnit(const_cast<CGC_AssemblyUnit &>(*Unit));
                Assm.Reset(unit_assm);
            }
        }
        if(!Assm) {
            return;
        }
    }

    switch (m_Info) {
    case eName:
        ostr << Assm->GetName();
        break;

    case eAccession:
        ostr << Assm->GetAccession();
        break;

    case eChainId:
        {{
             string accession = Assm->GetAccession();
             size_t chain_start = accession.find_first_of("123456789");
             size_t chain_end = accession.find('.');
             ostr << accession.substr(chain_start, chain_end-chain_start);
        }}
        break;

    case eChromosome:
        {{
            CConstRef<CGC_Sequence> top_level = Seq->GetTopLevelParent();
            if (top_level && top_level->HasRole(eGC_SequenceRole_chromosome) &&
                top_level->GetReplicon() &&
                top_level->GetReplicon()->IsSetName())
            {
                ostr << top_level->GetReplicon()->GetName();
            } else {
                ostr << "NA";
            }
        }}
        break;
    }
}

//////////////////////////////////////////////////////////////////////////////

CTabularFormatter_PatchType::CTabularFormatter_PatchType(int row, CConstRef<CGC_Assembly> gencoll)
: m_Row(row), m_Gencoll(gencoll)
{
}

void CTabularFormatter_PatchType::PrintHelpText(CNcbiOstream& ostr) const
{
    ostr << "Patch type, if any, of ";
    if (m_Row == 0) {
        ostr << "query";
    } else if (m_Row == 1) {
        ostr << "sequence";
    } else {
        NCBI_THROW(CException, eUnknown,
                   "only pairwise alignments are supported");
    }
    ostr << " sequence";
}

void CTabularFormatter_PatchType::PrintHeader(CNcbiOstream& ostr) const
{
    if (m_Row == 0) {
        ostr << "qpatchtype";
    } else if (m_Row == 1) {
        ostr << "spatchtype";
    } else {
        NCBI_THROW(CException, eUnknown,
                   "only pairwise alignments are supported");
    }
}

void CTabularFormatter_PatchType::Print(CNcbiOstream& ostr,
                                      const CSeq_align& align)
{
    if(!m_Gencoll)
        return;

    CConstRef<CGC_Sequence> Seq;
    Seq = m_Gencoll->Find(CSeq_id_Handle::GetHandle(align.GetSeq_id(m_Row)));
    if(!Seq)
        return;

    if(Seq->CanGetPatch_type()) {
        if(Seq->GetPatch_type() == CGC_Sequence::ePatch_type_fix)
            ostr << "FIX";
        else if(Seq->GetPatch_type() == CGC_Sequence::ePatch_type_novel)
            ostr << "NOVEL";
    }
}

//////////////////////////////////////////////////////////////////////////////

CTabularFormatter_NearestGap::CTabularFormatter_NearestGap(int row, CConstRef<CGC_Assembly> gencoll)
: m_Row(row), m_Gencoll(gencoll)
{
}

void CTabularFormatter_NearestGap::PrintHelpText(CNcbiOstream& ostr) const
{
    ostr << "Nearest Gap, if any, or edge, of ";
    if (m_Row == 0) {
        ostr << "query";
    } else if (m_Row == 1) {
        ostr << "subject";
    } else {
        NCBI_THROW(CException, eUnknown,
                   "only pairwise alignments are supported");
    }
    ostr << " sequence";
}

void CTabularFormatter_NearestGap::PrintHeader(CNcbiOstream& ostr) const
{
    if (m_Row == 0) {
        ostr << "qnearestgap";
    } else if (m_Row == 1) {
        ostr << "snearestgap";
    } else {
        NCBI_THROW(CException, eUnknown,
                   "only pairwise alignments are supported");
    }
}

TSeqPos s_FindGaps(const CGC_Assembly& Assembly, 
                   const CSeq_id& Id, 
                   const TSeqPos Offset, 
                   list<TSeqRange>& Gaps) 
{
    CConstRef<CGC_Sequence> Seq;
    CGC_Assembly::TSequenceList SeqList;
    Assembly.Find(CSeq_id_Handle::GetHandle(Id), SeqList);
    if(SeqList.empty())
        return 0;
    Seq = SeqList.front();

    if(!Seq)
        return 0;
    
    if(!Seq->CanGetStructure())
        return 0;

    TSeqPos CurrStart = Offset;
    ITERATE(CDelta_ext::Tdata, DeltaIter, Seq->GetStructure().Get()) {
        if( (*DeltaIter)->IsLiteral()) {
            if (!(*DeltaIter)->GetLiteral().CanGetSeq_data() ||
                 (*DeltaIter)->GetLiteral().GetSeq_data().IsGap()) {
                TSeqRange GapRange;
                GapRange.SetFrom(CurrStart);
                GapRange.SetLength((*DeltaIter)->GetLiteral().GetLength());
                Gaps.push_back(GapRange);
            }
            CurrStart += (*DeltaIter)->GetLiteral().GetLength();
        } else if( (*DeltaIter)->IsLoc()) {
            s_FindGaps(Assembly, *(*DeltaIter)->GetLoc().GetId(), CurrStart, Gaps);
            CurrStart += (*DeltaIter)->GetLoc().GetTotalRange().GetLength();
        }
    }
    return CurrStart;
}

void CTabularFormatter_NearestGap::Print(CNcbiOstream& ostr,
                                      const CSeq_align& align)
{
    if(!m_Gencoll) {
        ostr << "*";
        return;
    }


    list<TSeqRange> Gaps;
    TSeqPos SeqLength = s_FindGaps(*m_Gencoll, align.GetSeq_id(m_Row), 0, Gaps);

    if(SeqLength == 0) {
        ostr << "*";
        return;
    }

    TSeqRange CompRange = align.GetSeqRange(m_Row);
    TSeqPos MinGapDist = numeric_limits<TSeqPos>::max();
    MinGapDist = min(MinGapDist, (TSeqPos)abs((TSignedSeqPos)(CompRange.GetFrom()-0))); 
    MinGapDist = min(MinGapDist, (TSeqPos)abs((TSignedSeqPos)(CompRange.GetTo()-SeqLength))); 

    
    ITERATE(list<TSeqRange>, GapIter, Gaps) {
        MinGapDist = min(MinGapDist, (TSeqPos)abs((TSignedSeqPos)(CompRange.GetFrom()-GapIter->GetFrom()))); 
        MinGapDist = min(MinGapDist, (TSeqPos)abs((TSignedSeqPos)(CompRange.GetTo()-GapIter->GetTo())));
    }

    ostr << MinGapDist;
}


/////////////////////////////////////////////////////////////////////////////

CTabularFormatter_Traceback::CTabularFormatter_Traceback()
{
}


void CTabularFormatter_Traceback::PrintHelpText(CNcbiOstream& ostr) const
{
    ostr << "Blast Traceback string";
}

void CTabularFormatter_Traceback::PrintHeader(CNcbiOstream& ostr) const
{
    ostr << "btop";
}


void CTabularFormatter_Traceback::Print(CNcbiOstream& ostr,
                                        const CSeq_align& align)
{
    if(!align.CanGetSegs() || !align.GetSegs().IsDenseg()) {
        NCBI_THROW(CException, eUnknown,
                    "btop format only supports denseg alignments.");
    }

    ostr << m_Scores->GetTraceback(m_Scores->GetScope(), align, 0);
}


/////////////////////////////////////////////////////////////////////////////

CTabularFormatter::CTabularFormatter(CNcbiOstream& ostr, CScoreLookup &scores)
: m_Scores(&scores), m_Ostr(ostr)
{
    s_RegisterStandardFields(*this);
}

void CTabularFormatter::s_RegisterStandardFields(CTabularFormatter &formatter)
{
    IFormatter *qseqid =
        new CTabularFormatter_SeqId(0, sequence::eGetId_Best);
    formatter.RegisterField("qseqid", qseqid);
    formatter.RegisterField("qacc", qseqid);
    formatter.RegisterField("qaccver", qseqid);

    IFormatter *qallseqid =
        new CTabularFormatter_AllSeqIds(0);
    formatter.RegisterField("qallseqid", qallseqid);
    formatter.RegisterField("qallacc", qallseqid);

    formatter.RegisterField("qgi",
        new CTabularFormatter_SeqId(0, sequence::eGetId_ForceGi));
    formatter.RegisterField("qexactseqid",
        new CTabularFormatter_SeqId(0, sequence::eGetId_HandleDefault));
    
    formatter.RegisterField("qlen", new CTabularFormatter_SeqLength(0));
    formatter.RegisterField("qstrand", new CTabularFormatter_AlignStrand(0));
    formatter.RegisterField("qstart", new CTabularFormatter_AlignStart(0));
    formatter.RegisterField("qend", new CTabularFormatter_AlignEnd(0));
    formatter.RegisterField("qestart", new CTabularFormatter_AlignStart(0, true));
    formatter.RegisterField("qeend", new CTabularFormatter_AlignEnd(0, true));


    IFormatter *sseqid =
        new CTabularFormatter_SeqId(1, sequence::eGetId_Best);
    formatter.RegisterField("sseqid", sseqid);
    formatter.RegisterField("sacc", sseqid);
    formatter.RegisterField("saccver", sseqid);

    IFormatter *sallseqid =
        new CTabularFormatter_AllSeqIds(1);
    formatter.RegisterField("sallseqid", sallseqid);
    formatter.RegisterField("sallacc", sallseqid);

    formatter.RegisterField("sgi",
        new CTabularFormatter_SeqId(1, sequence::eGetId_ForceGi));
    formatter.RegisterField("sexactseqid",
        new CTabularFormatter_SeqId(1, sequence::eGetId_HandleDefault));
    
    formatter.RegisterField("slen", new CTabularFormatter_SeqLength(1));
    formatter.RegisterField("sstrand", new CTabularFormatter_AlignStrand(1));
    formatter.RegisterField("sstart", new CTabularFormatter_AlignStart(1));
    formatter.RegisterField("send", new CTabularFormatter_AlignEnd(1));
    formatter.RegisterField("sestart", new CTabularFormatter_AlignStart(1, true));
    formatter.RegisterField("seend", new CTabularFormatter_AlignEnd(1, true));
    
    formatter.RegisterField("evalue", new CTabularFormatter_EValue);
    formatter.RegisterField("evalue_mantissa", new CTabularFormatter_EValue_Mantissa);
    formatter.RegisterField("evalue_exponent", new CTabularFormatter_EValue_Exponent);
    formatter.RegisterField("bitscore", new CTabularFormatter_BitScore);
    formatter.RegisterField("score", new CTabularFormatter_Score);

    formatter.RegisterField("length", new CTabularFormatter_AlignLength);
    formatter.RegisterField("length_ungap", new CTabularFormatter_AlignLengthUngap);
    formatter.RegisterField("align_len_ratio", new CTabularFormatter_AlignLengthRatio);

    formatter.RegisterField("pident", new CTabularFormatter_PercentId(true));
    formatter.RegisterField("pident_ungapped", new CTabularFormatter_PercentId(false));
    formatter.RegisterField("pcov", new CTabularFormatter_PercentCoverage);

    formatter.RegisterField("gaps", new CTabularFormatter_GapBaseCount);
    formatter.RegisterField("gapopen", new CTabularFormatter_GapCount);

    formatter.RegisterField("nident", new CTabularFormatter_IdentityCount);
    formatter.RegisterField("mismatch", new CTabularFormatter_MismatchCount);
    formatter.RegisterField("qmismatchpos", new CTabularFormatter_MismatchPositions(0));
    formatter.RegisterField("smismatchpos", new CTabularFormatter_MismatchPositions(1));
    
    formatter.RegisterField("qgapranges", new CTabularFormatter_GapRanges(0));
    formatter.RegisterField("sgapranges", new CTabularFormatter_GapRanges(1));


    formatter.RegisterField("qdefline",
            new CTabularFormatter_Defline(0));
    formatter.RegisterField("sdefline",
            new CTabularFormatter_Defline(1));
    formatter.RegisterField("qprotref",
            new CTabularFormatter_ProtRef(0));
    formatter.RegisterField("sprotref",
            new CTabularFormatter_ProtRef(1));
    formatter.RegisterField("qtaxid",
            new CTabularFormatter_TaxId(0));
    formatter.RegisterField("staxid",
            new CTabularFormatter_TaxId(1));

    formatter.RegisterField("align_id",
            new CTabularFormatter_AlignId());
    formatter.RegisterField("best_placement_group",
            new CTabularFormatter_BestPlacementGroup());

    formatter.RegisterField("exons",
            new CTabularFormatter_ExonIntrons(1,
                                 CTabularFormatter_ExonIntrons::e_Exons,
                                 CTabularFormatter_ExonIntrons::e_Range));
    formatter.RegisterField("exon_len",
            new CTabularFormatter_ExonIntrons(1,
                                 CTabularFormatter_ExonIntrons::e_Exons,
                                 CTabularFormatter_ExonIntrons::e_Length));

    formatter.RegisterField("introns",
            new CTabularFormatter_ExonIntrons(1,
                                 CTabularFormatter_ExonIntrons::e_Introns,
                                 CTabularFormatter_ExonIntrons::e_Range));
    formatter.RegisterField("intron_len",
            new CTabularFormatter_ExonIntrons(1,
                                 CTabularFormatter_ExonIntrons::e_Introns,
                                 CTabularFormatter_ExonIntrons::e_Length));
    formatter.RegisterField("query_exons",
            new CTabularFormatter_ExonIntrons(0,
                                 CTabularFormatter_ExonIntrons::e_Exons,
                                 CTabularFormatter_ExonIntrons::e_Range));
    formatter.RegisterField("query_exon_len",
            new CTabularFormatter_ExonIntrons(0,
                                 CTabularFormatter_ExonIntrons::e_Exons,
                                 CTabularFormatter_ExonIntrons::e_Length));

    formatter.RegisterField("query_unaligned",
            new CTabularFormatter_ExonIntrons(0,
                                 CTabularFormatter_ExonIntrons::e_Introns,
                                 CTabularFormatter_ExonIntrons::e_Range));
    formatter.RegisterField("query_unaligned_len",
            new CTabularFormatter_ExonIntrons(0,
                                 CTabularFormatter_ExonIntrons::e_Introns,
                                 CTabularFormatter_ExonIntrons::e_Length));

    formatter.RegisterField("biggestgap",
            new CTabularFormatter_BiggestGapBases(CTabularFormatter_BiggestGapBases::e_All));
    formatter.RegisterField("qbiggestgap",
            new CTabularFormatter_BiggestGapBases(0));
    formatter.RegisterField("sbiggestgap",
            new CTabularFormatter_BiggestGapBases(1));
    formatter.RegisterField("qchrom",
            new CTabularFormatter_SeqChrom(0));
    formatter.RegisterField("schrom",
            new CTabularFormatter_SeqChrom(1));
    formatter.RegisterField("qclone",
            new CTabularFormatter_SeqClone(0));
    formatter.RegisterField("sclone",
            new CTabularFormatter_SeqClone(1));
    formatter.RegisterField("qtech",
            new CTabularFormatter_Tech(0));
    formatter.RegisterField("stech",
            new CTabularFormatter_Tech(1));
    formatter.RegisterField("qdiscstrand",
            new CTabularFormatter_DiscStrand(0));
    formatter.RegisterField("sdiscstrand",
            new CTabularFormatter_DiscStrand(1));
    formatter.RegisterField("cigar",
            new CTabularFormatter_Cigar);
    formatter.RegisterField("btop",
            new CTabularFormatter_Traceback);
    formatter.RegisterField("qasmunit", new CTabularFormatter_AssemblyInfo(0,
                                   CTabularFormatter_AssemblyInfo::eUnit,
                                   CTabularFormatter_AssemblyInfo::eName));
    formatter.RegisterField("sasmunit", new CTabularFormatter_AssemblyInfo(1,
                                   CTabularFormatter_AssemblyInfo::eUnit,
                                   CTabularFormatter_AssemblyInfo::eName));
    formatter.RegisterField("qfullasm", new CTabularFormatter_AssemblyInfo(0,
                                   CTabularFormatter_AssemblyInfo::eFull,
                                   CTabularFormatter_AssemblyInfo::eName));
    formatter.RegisterField("sfullasm", new CTabularFormatter_AssemblyInfo(1,
                                   CTabularFormatter_AssemblyInfo::eFull,
                                   CTabularFormatter_AssemblyInfo::eName));
    formatter.RegisterField("qasmunitacc", new CTabularFormatter_AssemblyInfo(0,
                                   CTabularFormatter_AssemblyInfo::eUnit,
                                   CTabularFormatter_AssemblyInfo::eAccession));
    formatter.RegisterField("sasmunitacc", new CTabularFormatter_AssemblyInfo(1,
                                   CTabularFormatter_AssemblyInfo::eUnit,
                                   CTabularFormatter_AssemblyInfo::eAccession));
    formatter.RegisterField("qfullasmacc", new CTabularFormatter_AssemblyInfo(0,
                                   CTabularFormatter_AssemblyInfo::eFull,
                                   CTabularFormatter_AssemblyInfo::eAccession));
    formatter.RegisterField("sfullasmacc", new CTabularFormatter_AssemblyInfo(1,
                                   CTabularFormatter_AssemblyInfo::eFull,
                                   CTabularFormatter_AssemblyInfo::eAccession));
    formatter.RegisterField("qasmunitchain", new CTabularFormatter_AssemblyInfo(0,
                                   CTabularFormatter_AssemblyInfo::eUnit,
                                   CTabularFormatter_AssemblyInfo::eChainId));
    formatter.RegisterField("sasmunitchain", new CTabularFormatter_AssemblyInfo(1,
                                   CTabularFormatter_AssemblyInfo::eUnit,
                                   CTabularFormatter_AssemblyInfo::eChainId));
    formatter.RegisterField("qfullasmchain", new CTabularFormatter_AssemblyInfo(0,
                                   CTabularFormatter_AssemblyInfo::eFull,
                                   CTabularFormatter_AssemblyInfo::eChainId));
    formatter.RegisterField("sfullasmchain", new CTabularFormatter_AssemblyInfo(1,
                                   CTabularFormatter_AssemblyInfo::eFull,
                                   CTabularFormatter_AssemblyInfo::eChainId));
    formatter.RegisterField("qchromosome", new CTabularFormatter_AssemblyInfo(0,
                                   CTabularFormatter_AssemblyInfo::eUnit,
                                   CTabularFormatter_AssemblyInfo::eChromosome));
    formatter.RegisterField("schromosome", new CTabularFormatter_AssemblyInfo(1,
                                   CTabularFormatter_AssemblyInfo::eUnit,
                                   CTabularFormatter_AssemblyInfo::eChromosome));
}

void CTabularFormatter::SetGencoll(CConstRef<CGC_Assembly> gencoll)
{
    NON_CONST_ITERATE (TFormatterMap, formatter_it, m_FormatterMap) {
        formatter_it->second->SetGencoll(gencoll);
    }
    RegisterField("qpatchtype", new CTabularFormatter_PatchType(0, gencoll));
    RegisterField("spatchtype", new CTabularFormatter_PatchType(1, gencoll));
    RegisterField("qnearestgap", new CTabularFormatter_NearestGap(0, gencoll));
    RegisterField("snearestgap", new CTabularFormatter_NearestGap(1, gencoll));
}

/// Split a string, but ignore separators within parentheses
static void s_Split(const string &format,
                    const string &separators,
                    vector<string> &toks)
{
    unsigned int paren_level = 0;
    string next_tok;
    ITERATE (string, char_it, format) {
        if (!paren_level && separators.find(*char_it) != string::npos) {
            if (!next_tok.empty()) {
                toks.push_back(next_tok);
            }
            next_tok.clear();
            continue;
        }
        if (*char_it == '(') {
            ++paren_level;
        } else if (*char_it == ')') {
            if (!paren_level) {
                NCBI_THROW(CException, eUnknown,
                           "Unbalanced parentheses: " + format);
            }
            --paren_level;
        }
        next_tok += *char_it;
    }
    if (!next_tok.empty()) {
        toks.push_back(next_tok);
    }
    if (paren_level) {
        NCBI_THROW(CException, eUnknown,
                   "Unbalanced parentheses: " + format);
    }
}

void CTabularFormatter::SetFormat(const string& format)
{
    CRegexp re1("score\\(([^,]*),([^)]*)\\)");
    CRegexp re2("score\\(([^)]*)\\)");

    CRegexp text_re1("text\\(([^,]*),([^)]*)\\)");
    CRegexp text_re2("text\\(([^)]*)\\)");

    vector<string> toks;
    s_Split(format, " \t\n\r,", toks);

    ITERATE (vector<string>, it, toks) {
        string s = *it;
        NStr::ToLower(s);
        if (m_FormatterMap.count(s)) {
            m_Formatters.push_back(m_FormatterMap[s]);
        } else if (re1.IsMatch(s)) {
            string score_name = re1.GetSub(*it, 1);
            string col_name = re1.GetSub(*it, 2);
            m_Formatters.push_back(CIRef<IFormatter>(new CTabularFormatter_AnyScore(score_name, col_name)));

        } else if (re2.IsMatch(s)) {
            string score_name = re2.GetSub(*it, 1);
            m_Formatters.push_back(CIRef<IFormatter>(new CTabularFormatter_AnyScore(score_name, score_name)));

        } else if (text_re1.IsMatch(s)) {
            string score_name = text_re1.GetSub(*it, 1);
            string col_name = text_re1.GetSub(*it, 2);
            m_Formatters.push_back(CIRef<IFormatter>(new CTabularFormatter_FixedText(score_name, col_name)));

        } else if (text_re2.IsMatch(s)) {
            string score_name = text_re2.GetSub(*it, 1);
            m_Formatters.push_back(CIRef<IFormatter>(new CTabularFormatter_FixedText(score_name, score_name)));

        } else {
            LOG_POST(Error << "unhandled field: " << s);
        }
    }

    NON_CONST_ITERATE (list< CIRef<IFormatter> >, it, m_Formatters) {
        (*it)->SetScoreLookup(m_Scores);
    }
}


void CTabularFormatter::WriteHeader()
{
    m_Ostr << '#';
    NON_CONST_ITERATE (list< CIRef<IFormatter> >, it, m_Formatters) {
        (*it)->PrintHeader(m_Ostr);

        list< CIRef<IFormatter> >::const_iterator i = it;
        ++i;
        if (i != m_Formatters.end()) {
            m_Ostr << '\t';
        }
    }

    m_Ostr << '\n';
}


void CTabularFormatter::Format(const CSeq_align& align)
{
    NON_CONST_ITERATE (list< CIRef<IFormatter> >, it, m_Formatters) {
        (*it)->Print(m_Ostr, align);

        list< CIRef<IFormatter> >::const_iterator i = it;
        ++i;
        if (i != m_Formatters.end()) {
            m_Ostr << '\t';
        }
    }
    m_Scores->UpdateState(align);

    m_Ostr << '\n';
}

END_SCOPE(ncbi)

