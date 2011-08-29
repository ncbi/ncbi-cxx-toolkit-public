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
 * Authors:  Mike DiCuccio
 *
 * File Description:
 *
 */

#include <ncbi_pch.hpp>
#include <algo/align/util/align_filter.hpp>
#include <algo/align/util/score_builder.hpp>
#include <algo/sequence/gene_model.hpp>
#include <corelib/rwstream.hpp>

#include <objects/seqalign/Seq_align_set.hpp>
#include <objects/seqalign/Seq_align.hpp>
#include <objects/seqalign/Dense_seg.hpp>
#include <objects/seqalign/Score.hpp>
#include <objects/seqalign/Spliced_seg.hpp>
#include <objects/seqalign/Spliced_exon_chunk.hpp>
#include <objects/seqalign/Product_pos.hpp>
#include <objects/seqalign/Prot_pos.hpp>
#include <objects/general/Object_id.hpp>
#include <objects/seq/Seq_annot.hpp>

#include <util/checksum.hpp>
#include <math.h>

#include <objmgr/object_manager.hpp>
#include <objmgr/scope.hpp>
#include <objmgr/impl/synonyms.hpp>
#include <objmgr/util/sequence.hpp>

BEGIN_NCBI_SCOPE
USING_SCOPE(objects);


/////////////////////////////////////////////////////////////////////////////

class CScore_AlignLength : public CAlignFilter::IScore
{
public:
    CScore_AlignLength(bool include_gaps)
        : m_Gaps(include_gaps)
    {
    }

    virtual EComplexity GetComplexity() const { return eEasy; };

    virtual double Get(const CSeq_align& align, CScope*) const
    {
        return align.GetAlignLength(m_Gaps);
    }

    virtual void PrintHelp(CNcbiOstream& ostr) const
    {
        if (m_Gaps) {
            ostr << "Length of the aligned segments, including the length of all gap segments";
        }
        else {
            ostr << "Length of the aligned segments, excluding all gap segments; thus, this is the length of all actually aligned (i.e., match or mismatch) bases";
        }
    }

private:
    bool m_Gaps;
};

/////////////////////////////////////////////////////////////////////////////

class CScore_LongestGapLength : public CAlignFilter::IScore
{
public:
    virtual void PrintHelp(CNcbiOstream& ostr) const
    {
        ostr << "Length of the longest gap observed in either query or subject";
    }

    virtual EComplexity GetComplexity() const { return eEasy; };

    virtual double Get(const CSeq_align& align, CScope* s) const
    {
        try {
            return align.GapLengthRange().second;
        } catch (CSeqalignException &) {
            return numeric_limits<double>::quiet_NaN();
        }
    }
};

/////////////////////////////////////////////////////////////////////////////

class CScore_3PrimeUnaligned : public CAlignFilter::IScore
{
public:

    virtual void PrintHelp(CNcbiOstream& ostr) const
    {
        ostr << "Length of unaligned sequence 3' of alignment end";
    }

    virtual EComplexity GetComplexity() const { return eEasy; };

    virtual double Get(const CSeq_align& align, CScope* scope) const
    {
        double score_value = 0;
        if (align.GetSegs().IsSpliced()) {
            score_value = align.GetSegs().GetSpliced().GetProduct_length();
            if (align.GetSegs().GetSpliced().IsSetPoly_a()) {
                score_value = align.GetSegs().GetSpliced().GetPoly_a();
            }
        } else {
            if (scope) {
                CBioseq_Handle bsh = scope->GetBioseqHandle(align.GetSeq_id(0));
                if (bsh) {
                    score_value = bsh.GetBioseqLength();
                }
            }
        }
        if (score_value) {
            score_value -= align.GetSeqStop(0) + 1;
        }
        return score_value;
    }
};

/////////////////////////////////////////////////////////////////////////////

class CScore_InternalUnaligned : public CAlignFilter::IScore
{
public:

    virtual void PrintHelp(CNcbiOstream& ostr) const
    {
        ostr << "Length of unaligned sequence contained within the aligned "
            "range.  Note that this does not count gaps; rather, it computes "
            "the length of all missing, unaligned sequence bounded by the "
            "aligned range";
    }

    virtual EComplexity GetComplexity() const { return eEasy; };

    virtual double Get(const CSeq_align& align, CScope* scope) const
    {
        double score_value = 0;
        switch (align.GetSegs().Which()) {
        case CSeq_align::TSegs::e_Spliced:
            {{
                 const CSpliced_seg& seg = align.GetSegs().GetSpliced();
                 if (seg.IsSetProduct_strand()  &&
                     seg.GetProduct_strand() == eNa_strand_minus) {
                     CSpliced_seg::TExons::const_reverse_iterator it =
                         seg.GetExons().rbegin();
                     CSpliced_seg::TExons::const_reverse_iterator prev =
                         seg.GetExons().rbegin();
                     CSpliced_seg::TExons::const_reverse_iterator end =
                         seg.GetExons().rend();
                     if (seg.GetProduct_type() ==
                         CSpliced_seg::eProduct_type_transcript) {
                         for (++it;  it != end;  ++it, ++prev) {
                             score_value += (*it)->GetProduct_start().GetNucpos() -
                                 (*prev)->GetProduct_end().GetNucpos() - 1;
                         }
                     } else {
                         for (++it;  it != end;  ++it, ++prev) {
                             const CProt_pos& curr =
                                 (*it)->GetProduct_start().GetProtpos();
                             const CProt_pos& last =
                                 (*prev)->GetProduct_end().GetProtpos();
                             TSeqPos curr_nuc = curr.GetAmin() * 3;
                             if (curr.GetFrame()) {
                                 curr_nuc += curr.GetFrame() - 1;
                             }
                             TSeqPos last_nuc = last.GetAmin() * 3;
                             if (last.GetFrame()) {
                                 last_nuc += last.GetFrame() - 1;
                             }
                             score_value += curr_nuc - last_nuc - 1;
                         }
                     }
                 }
                 else {
                     CSpliced_seg::TExons::const_iterator it =
                         seg.GetExons().begin();
                     CSpliced_seg::TExons::const_iterator prev =
                         seg.GetExons().begin();
                     CSpliced_seg::TExons::const_iterator end =
                         seg.GetExons().end();
                     if (seg.GetProduct_type() ==
                         CSpliced_seg::eProduct_type_transcript) {
                         for (++it;  it != end;  ++it, ++prev) {
                             score_value += (*it)->GetProduct_start().GetNucpos() -
                                 (*prev)->GetProduct_end().GetNucpos() - 1;
                         }
                     } else {
                         for (++it;  it != end;  ++it, ++prev) {
                             const CProt_pos& curr =
                                 (*it)->GetProduct_start().GetProtpos();
                             const CProt_pos& last =
                                 (*prev)->GetProduct_end().GetProtpos();
                             TSeqPos curr_nuc = curr.GetAmin() * 3;
                             if (curr.GetFrame()) {
                                 curr_nuc += curr.GetFrame() - 1;
                             }
                             TSeqPos last_nuc = last.GetAmin() * 3;
                             if (last.GetFrame()) {
                                 last_nuc += last.GetFrame() - 1;
                             }
                             score_value += curr_nuc - last_nuc - 1;
                         }
                     }
                 }
             }}
            break;

        default:
            NCBI_THROW(CException, eUnknown,
                       "internal_unaligned not implemented for this "
                       "type of alignment");
        }
        return score_value;
    }
};

/////////////////////////////////////////////////////////////////////////////

class CScore_AlignStartStop : public CAlignFilter::IScore
{
public:
    CScore_AlignStartStop(int row, bool start)
        : m_Row(row)
        , m_Start(start)
    {
    }

    virtual EComplexity GetComplexity() const { return eEasy; };

    virtual void PrintHelp(CNcbiOstream& ostr) const
    {
        if (m_Start) {
            if (m_Row == 0) {
                ostr << "Start of query sequence (0-based coordinates)";
            }
            else if (m_Row == 1) {
                ostr << "Start of subject sequence (0-based coordinates)";
            }
        }
        else {
            if (m_Row == 0) {
                ostr << "End of query sequence (0-based coordinates)";
            }
            else if (m_Row == 1) {
                ostr << "End of subject sequence (0-based coordinates)";
            }
        }
    }

    virtual double Get(const CSeq_align& align, CScope*) const
    {
        if (m_Start) {
            return align.GetSeqStart(m_Row);
        } else {
            return align.GetSeqStop(m_Row);
        }
    }

private:
    int m_Row;
    bool m_Start;
};

/////////////////////////////////////////////////////////////////////////////

class CScore_AlignLengthRatio : public CAlignFilter::IScore
{
public:

    virtual void PrintHelp(CNcbiOstream& ostr) const
    {
        ostr << "Ratio of subject aligned range length to query aligned "
            "range length";
    }

    virtual EComplexity GetComplexity() const { return eEasy; };

    virtual double Get(const CSeq_align& align, CScope*) const
    {
        return align.AlignLengthRatio();
    }
};

//////////////////////////////////////////////////////////////////////////////

class CScore_SequenceLength : public CAlignFilter::IScore
{
public:
    CScore_SequenceLength(int row)
        : m_Row(row)
    {
    }

    virtual void PrintHelp(CNcbiOstream& ostr) const
    {
        if (m_Row == 0) {
            ostr << "Length of query sequence";
        }
        else if (m_Row == 1) {
            ostr << "Length of subject sequence";
        }
    }

    virtual EComplexity GetComplexity() const { return eHard; };

    virtual double Get(const CSeq_align& align, CScope* scope) const
    {
        double score = numeric_limits<double>::quiet_NaN();
        if (m_Row == 0  &&  align.GetSegs().IsSpliced()) {
            score = align.GetSegs().GetSpliced().GetProduct_length();
        } else {
            if (scope) {
                CBioseq_Handle bsh =
                    scope->GetBioseqHandle(align.GetSeq_id(m_Row));
                if (bsh) {
                    score = bsh.GetBioseqLength();
                }
            }
        }
        return score;
    }

private:
    int m_Row;
    bool m_Start;
};

//////////////////////////////////////////////////////////////////////////////

class CScore_SymmetricOverlap : public CAlignFilter::IScore
{
public:
    virtual void PrintHelp(CNcbiOstream& ostr) const
    {
        ostr <<
            "Symmetric overlap, as a percent (0-100).  This is similar to "
            "coverage, except that it takes into account both query and "
            "subject sequence lengths";
    }

    virtual EComplexity GetComplexity() const { return eHard; };

    virtual double Get(const CSeq_align& align, CScope* scope) const
    {
        double pct_overlap = 0;
        try {
            TSeqPos length = align.GetAlignLength(false);
            CBioseq_Handle q = scope->GetBioseqHandle(align.GetSeq_id(0));
            CBioseq_Handle s = scope->GetBioseqHandle(align.GetSeq_id(1));

            pct_overlap = 2 * length;
            pct_overlap /= (q.GetBioseqLength() + s.GetBioseqLength());
            pct_overlap *= 100;
        }
        catch (CException &e) {
        }
        return pct_overlap;
    }
};

//////////////////////////////////////////////////////////////////////////////

class CScore_MinExonLength : public CAlignFilter::IScore
{
public:
    virtual void PrintHelp(CNcbiOstream& ostr) const
    {
        ostr <<
            "Length of the shortest exon.  Note that this score has "
            "meaning only for Spliced-seg alignments, as would be generated "
            "by Splign or ProSplign.";
    }

    virtual EComplexity GetComplexity() const { return eEasy; };

    virtual double Get(const CSeq_align& align, CScope*) const
    {
        try {
            return align.ExonLengthRange().first;
        } catch (CSeqalignException &) {
            return numeric_limits<double>::quiet_NaN();
        }
    }
};

//////////////////////////////////////////////////////////////////////////////

class CScore_MaxIntronLength : public CAlignFilter::IScore
{
public:
    virtual void PrintHelp(CNcbiOstream& ostr) const
    {
        ostr <<
            "Length of the longest intron.  Note that this score has "
            "meaning only for Spliced-seg alignments, as would be generated "
            "by Splign or ProSplign.";
    }

    virtual EComplexity GetComplexity() const { return eEasy; };

    virtual double Get(const CSeq_align& align, CScope*) const
    {
        try {
            return align.IntronLengthRange().second;
        } catch (CSeqalignException &) {
            return numeric_limits<double>::quiet_NaN();
        }
    }
};

//////////////////////////////////////////////////////////////////////////////

class CScore_ExonCount : public CAlignFilter::IScore
{
public:
    virtual void PrintHelp(CNcbiOstream& ostr) const
    {
        ostr <<
            "Count of the number of exons.  Note that this score has "
            "meaning only for Spliced-seg alignments, as would be generated "
            "by Splign or ProSplign.";
    }

    virtual EComplexity GetComplexity() const { return eEasy; };

    virtual double Get(const CSeq_align& align, CScope*) const
    {
        if (align.GetSegs().IsSpliced()) {
            const CSpliced_seg& seg = align.GetSegs().GetSpliced();
            if (seg.IsSetExons()) {
                return seg.GetExons().size();
            }
            return 0;
        }

        NCBI_THROW(CException, eUnknown,
                   "'exon_count' score is valid only for "
                   "Spliced-seg alignments");
    }
};

//////////////////////////////////////////////////////////////////////////////

class CScore_CdsInternalStops : public CAlignFilter::IScore
{
public:
    virtual void PrintHelp(CNcbiOstream& ostr) const
    {
        ostr <<
            "Count of the number of internal stop codons encountered when "
            "translating the coding region associated with the aligned "
            "transcript.  Note that this has meaning only for Spliced-seg "
            "transcript alignments.";
    }

    virtual EComplexity GetComplexity() const { return eHard; };

    virtual double Get(const CSeq_align& align, CScope* scope) const
    {
        double score = 0;

        try {
            ///
            /// complicated
            ///

            /// first, generate a gene model
            CFeatureGenerator generator(*scope);
            generator.SetFlags(CFeatureGenerator::fDefaults |
                               CFeatureGenerator::fGenerateLocalIds);
            generator.SetAllowedUnaligned(10);

            CConstRef<CSeq_align> clean_align = generator.CleanAlignment(align);
            CSeq_annot annot;
            CBioseq_set bset;
            generator.ConvertAlignToAnnot(*clean_align, annot, bset);

            /// extract the CDS and translate it
            CRef<CSeq_feat> cds;
            ITERATE (CSeq_annot::TData::TFtable, it, annot.GetData().GetFtable()) {
                if ((*it)->GetData().Which() == CSeqFeatData::e_Cdregion) {
                    cds = *it;
                    break;
                }
            }

            if (cds) {
                string trans;
                CSeqTranslator::Translate(*cds, *scope, trans);
                if ( !cds->GetLocation().IsPartialStop(eExtreme_Biological)  &&
                     NStr::EndsWith(trans, "*")) {
                    trans.resize(trans.size() - 1);
                }

                ITERATE (string, i, trans) {
                    score += (*i == '*');
                }
            }
        }
        catch (CException& e) {
            CNcbiOstrstream os;
            os << MSerial_AsnText << align;
            ERR_POST(Error << "error computing internal stops: " << e);
            ERR_POST(Error << "source alignment: "
                     << string(CNcbiOstrstreamToString(os)));
            ERR_POST(Error << "proceeding with " << score << " internal stops");
        }
        return score;
    }
};

/////////////////////////////////////////////////////////////////////////////

class CScore_CdsScore : public CAlignFilter::IScore
{
public:
    enum EScoreType {ePercentIdentity, ePercentCoverage, eStart, eEnd};

    CScore_CdsScore(EScoreType type)
    : m_ScoreType(type)
    {}

    virtual EComplexity GetComplexity() const { return eHard; };

    virtual void PrintHelp(CNcbiOstream& ostr) const
    {
        switch (m_ScoreType) {
        case ePercentIdentity:
            ostr <<
                "Percent-identity score confined to the coding region "
                "associated with the align transcipt. Not supported "
                "for standard-seg alignments.";
            break;
        case ePercentCoverage:
            ostr <<
                "Percent-coverage score confined to the coding region "
                "associated with the align transcipt.";
            break;
        case eStart:
            ostr << "Start position of product's coding region.";
            break; 
        case eEnd:
            ostr << "End position of product's coding region.";
            break; 
        }
        ostr << " Note that this has meaning only if product has a coding "
                "region annotation.";
    }

    virtual double Get(const CSeq_align& align, CScope* scope) const
    {
        double score = -1;
        if (align.GetSegs().IsStd()) {
            return score;
        }

        CBioseq_Handle product = scope->GetBioseqHandle(align.GetSeq_id(0));
        CFeat_CI cds(product, CSeqFeatData::eSubtype_cdregion);

        if (cds) {
            switch (m_ScoreType) {
            case eStart:
                score = cds->GetLocation().GetStart(eExtreme_Positional);
                break;

            case eEnd:
                score = cds->GetLocation().GetStop(eExtreme_Positional);
                break;

            default:
            {{
                CRangeCollection<TSeqPos> cds_ranges;
                for (CSeq_loc_CI it(cds->GetLocation()); it; ++it) {
                    cds_ranges += it.GetRange();
                }
                score = m_ScoreType == ePercentIdentity
                        ? CScoreBuilder().GetPercentIdentity(*scope, align,
                                                            cds_ranges)
                        : CScoreBuilder().GetPercentCoverage(*scope, align,
                                                            cds_ranges);
                break;
            }}
            }
        }
        return score;
    }

private:
    const EScoreType m_ScoreType;
};

//////////////////////////////////////////////////////////////////////////////

class CScore_Taxid : public CAlignFilter::IScore
{
public:
    CScore_Taxid(int row)
        : m_Row(row)
    {
    }

    virtual EComplexity GetComplexity() const { return eHard; };

    virtual void PrintHelp(CNcbiOstream& ostr) const
    {
        if (m_Row == 0) {
            ostr << "Taxid of query sequence";
        }
        else if (m_Row == 1) {
            ostr << "Taxid of subject sequence";
        }
    }

    virtual double Get(const CSeq_align& align, CScope* scope) const
    {
        return sequence::GetTaxId(
                   scope->GetBioseqHandle(align.GetSeq_id(m_Row)));
    }

private:
    int m_Row;
};

//////////////////////////////////////////////////////////////////////////////

class CScore_LastSpliceSite : public CAlignFilter::IScore
{
public:
    CScore_LastSpliceSite()
    {
    }

    virtual void PrintHelp(CNcbiOstream& ostr) const
    {
        ostr <<
            "Position of last splice site.  Note that this has meaning only "
            "for Spliced-seg transcript alignments, and only if the alignment "
            "has at least two exons.";
    }

    virtual EComplexity GetComplexity() const { return eEasy; };

    virtual double Get(const CSeq_align& align, CScope* scope) const
    {
        double score = numeric_limits<double>::quiet_NaN();
        if (align.GetSegs().IsSpliced())
        {
            const CSpliced_seg &seg = align.GetSegs().GetSpliced();
            if (seg.CanGetExons() && seg.GetExons().size() > 1 &&
                seg.CanGetProduct_type() &&
                seg.GetProduct_type() == CSpliced_seg::eProduct_type_transcript &&
                seg.CanGetProduct_strand() &&
                seg.GetProduct_strand() != eNa_strand_unknown)
            {
                const CSpliced_exon &last_spliced_exon =
                    seg.GetProduct_strand() == eNa_strand_minus
                        ? **++align.GetSegs().GetSpliced().GetExons().begin()
                        : **++align.GetSegs().GetSpliced().GetExons().rbegin();
                if (last_spliced_exon.CanGetProduct_end()) {
                    score = last_spliced_exon.GetProduct_end().GetNucpos();
                }
            }
        }
        return score;
    }

private:
    int m_Row;
};


//////////////////////////////////////////////////////////////////////////////

class CScore_Overlap : public CAlignFilter::IScore
{
public:
    CScore_Overlap(int row, bool include_gaps)
    : m_Row(row)
    , m_IncludeGaps(include_gaps)
    {
    }

    virtual void PrintHelp(CNcbiOstream& ostr) const
    {
        string row_name = m_Row == 0 ? "query" : "subject";
        string range_type = m_IncludeGaps ? "total aligned range" : "aligned bases";
        ostr <<
            "size of overlap of " + range_type + " with any alignments "
            "over the same " + row_name + " sequence that have previously "
            "passed this filter. Assumes that input alignments "
            "are collated by " + row_name + ", and then sorted by priority for "
            "inclusion in the output.";
    }

    virtual EComplexity GetComplexity() const { return eEasy; };

    virtual double Get(const CSeq_align& align, CScope* ) const
    {
        CRangeCollection<TSeqPos> overlap;
        if (align.GetSeq_id(m_Row).Match(m_CurrentSeq)) {
            overlap = m_CoveredRanges;
            if (m_IncludeGaps) {
                overlap &= align.GetSeqRange(m_Row);
            } else {
                overlap &= align.GetAlignedBases(m_Row);
            }
        }
        return overlap.GetCoveredLength();
    }

    virtual void UpdateState(const objects::CSeq_align& align)
    {
        const CSeq_id &aligned_id = align.GetSeq_id(m_Row);
        if (!aligned_id.Match(m_CurrentSeq)) {
            m_CurrentSeq.Assign(aligned_id);
            m_CoveredRanges.clear();
        }
        if (m_IncludeGaps) {
            m_CoveredRanges += align.GetSeqRange(m_Row);
        } else {
            m_CoveredRanges += align.GetAlignedBases(m_Row);
        }
    }

private:
    int m_Row;
    bool m_IncludeGaps;
    CSeq_id m_CurrentSeq;
    CRangeCollection<TSeqPos> m_CoveredRanges;
};


///////////////////////////////////////////////////////////////////////////////

static inline void s_ParseTree_Flatten(CQueryParseTree& tree,
                                       CQueryParseTree::TNode& node)
{
    CQueryParseNode::EType type = node->GetType();
    switch (type) {
    case CQueryParseNode::eAnd:
    case CQueryParseNode::eOr:
        {{
             CQueryParseTree::TNode::TNodeList_I iter;
             size_t hoisted = 0;
             do {
                 hoisted = 0;
                 for (iter = node.SubNodeBegin();
                      iter != node.SubNodeEnd();  ) {
                     CQueryParseTree::TNode& sub_node = **iter;
                     if (sub_node->GetType() == type) {
                         /// hoist this node's children
                         CQueryParseTree::TNode::TNodeList_I sub_iter =
                             sub_node.SubNodeBegin();
                         for ( ;  sub_iter != sub_node.SubNodeEnd(); ) {
                             node.AddNode(sub_node.DetachNode(*sub_iter++));
                         }

                         node.RemoveNode(iter++);
                         ++hoisted;
                     } else {
                         ++iter;
                     }
                 }
             }
             while (hoisted != 0);
         }}
        break;

    default:
        break;
    }

    CQueryParseTree::TNode::TNodeList_I iter;
    for (iter = node.SubNodeBegin();
         iter != node.SubNodeEnd();  ++iter) {
        s_ParseTree_Flatten(tree, **iter);
    }
}



//////////////////////////////////////////////////////////////////////////////

CAlignFilter::CAlignFilter()
    : m_RemoveDuplicates(false)
    , m_IsDryRun(false)
{
    x_Init();
}


CAlignFilter::CAlignFilter(const string& query)
    : m_RemoveDuplicates(false)
    , m_IsDryRun(false)
{
    x_Init();
    SetFilter(query);
}


void CAlignFilter::x_Init()
{
    m_Scores.insert
        (TScoreDictionary::value_type
         ("align_length",
          CIRef<IScore>(new CScore_AlignLength(true /* include gaps */))));
    m_Scores.insert
        (TScoreDictionary::value_type
         ("align_length_ungap",
          CIRef<IScore>(new CScore_AlignLength(false /* include gaps */))));
    m_Scores.insert
        (TScoreDictionary::value_type
         ("symmetric_overlap",
          CIRef<IScore>(new CScore_SymmetricOverlap)));
    m_Scores.insert
        (TScoreDictionary::value_type
         ("3prime_unaligned",
          CIRef<IScore>(new CScore_3PrimeUnaligned)));

    m_Scores.insert
        (TScoreDictionary::value_type
         ("min_exon_len",
          CIRef<IScore>(new CScore_MinExonLength)));

    m_Scores.insert
        (TScoreDictionary::value_type
         ("max_intron_len",
          CIRef<IScore>(new CScore_MaxIntronLength)));

    m_Scores.insert
        (TScoreDictionary::value_type
         ("longest_gap",
          CIRef<IScore>(new CScore_LongestGapLength)));

    {{
         CIRef<IScore> score(new CScore_AlignStartStop(0, true));
         m_Scores.insert
             (TScoreDictionary::value_type
              ("query_start", score));
         m_Scores.insert
             (TScoreDictionary::value_type
              ("5prime_unaligned", score));
         m_Scores.insert
             (TScoreDictionary::value_type
              ("query_end",
               CIRef<IScore>(new CScore_AlignStartStop(0, false))));
     }}

    m_Scores.insert
        (TScoreDictionary::value_type
         ("internal_unaligned",
          CIRef<IScore>(new CScore_InternalUnaligned)));

    m_Scores.insert
        (TScoreDictionary::value_type
         ("cds_internal_stops",
          CIRef<IScore>(new CScore_CdsInternalStops)));
    m_Scores.insert
        (TScoreDictionary::value_type
         ("cds_start",
          CIRef<IScore>(new CScore_CdsScore(CScore_CdsScore::eStart))));
    m_Scores.insert
        (TScoreDictionary::value_type
         ("cds_end",
          CIRef<IScore>(new CScore_CdsScore(CScore_CdsScore::eEnd))));
    m_Scores.insert
        (TScoreDictionary::value_type
         ("cds_pct_identity",
          CIRef<IScore>(new CScore_CdsScore(CScore_CdsScore::ePercentIdentity))));
    m_Scores.insert
        (TScoreDictionary::value_type
         ("cds_pct_coverage",
          CIRef<IScore>(new CScore_CdsScore(CScore_CdsScore::ePercentCoverage))));

    m_Scores.insert
        (TScoreDictionary::value_type
         ("align_length_ratio",
          CIRef<IScore>(new CScore_AlignLengthRatio)));

    m_Scores.insert
        (TScoreDictionary::value_type
         ("subject_start",
          CIRef<IScore>(new CScore_AlignStartStop(1, true))));
    m_Scores.insert
        (TScoreDictionary::value_type
         ("subject_end",
          CIRef<IScore>(new CScore_AlignStartStop(1, false))));

    {{
         CIRef<IScore> score(new CScore_SequenceLength(0));
         m_Scores.insert
             (TScoreDictionary::value_type
              ("query_length", score));
         m_Scores.insert
             (TScoreDictionary::value_type
              ("product_length", score));
         m_Scores.insert
             (TScoreDictionary::value_type
              ("subject_length",
               CIRef<IScore>(new CScore_SequenceLength(1))));
     }}

    m_Scores.insert
        (TScoreDictionary::value_type
         ("query_taxid",
          CIRef<IScore>(new CScore_Taxid(0))));
    m_Scores.insert
        (TScoreDictionary::value_type
         ("subject_taxid",
          CIRef<IScore>(new CScore_Taxid(1))));

    m_Scores.insert
        (TScoreDictionary::value_type
         ("last_splice_site",
          CIRef<IScore>(new CScore_LastSpliceSite)));

    m_Scores.insert
        (TScoreDictionary::value_type
         ("exon_count",
          CIRef<IScore>(new CScore_ExonCount)));

    m_Scores.insert
        (TScoreDictionary::value_type
         ("query_overlap",
          CIRef<IScore>(new CScore_Overlap(0, true))));

    m_Scores.insert
        (TScoreDictionary::value_type
         ("subject_overlap",
          CIRef<IScore>(new CScore_Overlap(1, true))));

    m_Scores.insert
        (TScoreDictionary::value_type
         ("query_overlap_nogaps",
          CIRef<IScore>(new CScore_Overlap(0, false))));

    m_Scores.insert
        (TScoreDictionary::value_type
         ("subject_overlap_nogaps",
          CIRef<IScore>(new CScore_Overlap(1, false))));
}


void CAlignFilter::SetFilter(const string& filter)
{
    static const char *sc_Functions[] = {
        "MUL",
        "DIV",
        "ADD",
        "SUB",
        "IS_SEG_TYPE",
        "COALESCE",
        NULL
    };

    m_Query = filter;
    m_ParseTree.reset(new CQueryParseTree);

    vector<string> func_vec;
    for (const char** func = sc_Functions;  func  &&  *func;  ++func) {
        func_vec.push_back(*func);
    }

    m_ParseTree->Parse(m_Query.c_str(),
                       CQueryParseTree::eCaseInsensitive,
                       CQueryParseTree::eSyntaxCheck, false,
                       func_vec);

    // flatten the tree
    // this transforms the tree so that equivalent nodes are grouped more
    // effectively.  this grouping permist easier tree evaluation
    s_ParseTree_Flatten(*m_ParseTree, *m_ParseTree->GetQueryTree());

    m_Scope.Reset(new CScope(*CObjectManager::GetInstance()));
    m_Scope->AddDefaults();
}

void CAlignFilter::SetScope(CScope& scope)
{
    m_Scope.Reset(&scope);
}


CScope& CAlignFilter::SetScope()
{
    return *m_Scope;
}

void CAlignFilter::AddBlacklistQueryId(const CSeq_id_Handle& idh)
{
    m_QueryBlacklist.insert(idh);
}


void CAlignFilter::AddWhitelistQueryId(const CSeq_id_Handle& idh)
{
    m_QueryWhitelist.insert(idh);
}


void CAlignFilter::AddBlacklistSubjectId(const CSeq_id_Handle& idh)
{
    m_SubjectBlacklist.insert(idh);
}


void CAlignFilter::AddWhitelistSubjectId(const CSeq_id_Handle& idh)
{
    m_SubjectWhitelist.insert(idh);
}


CAlignFilter& CAlignFilter::SetRemoveDuplicates(bool b)
{
    m_RemoveDuplicates = b;
    return *this;
}


void CAlignFilter::Filter(const list< CRef<CSeq_align> >& aligns_in,
                          list< CRef<CSeq_align> >& aligns_out)
{
    ITERATE (list< CRef<CSeq_align> >, iter, aligns_in) {
        if (Match(**iter)) {
            aligns_out.push_back(*iter);
        }
    }
}


void CAlignFilter::Filter(const CSeq_align_set& aligns_in,
                          CSeq_align_set&       aligns_out)
{
    Filter(aligns_in.Get(), aligns_out.Set());
}


void CAlignFilter::Filter(const CSeq_annot& aligns_in,
                          CSeq_annot&       aligns_out)
{
    Filter(aligns_in.GetData().GetAlign(), aligns_out.SetData().SetAlign());
}


bool CAlignFilter::Match(const CSeq_align& align)
{
    if (align.CheckNumRows() == 2) {
        if (m_QueryBlacklist.size()  ||  m_QueryWhitelist.size()) {
            CSeq_id_Handle query =
                CSeq_id_Handle::GetHandle(align.GetSeq_id(0));
            if (m_Scope) {
                CSeq_id_Handle idh =
                    sequence::GetId(query, *m_Scope,
                                    sequence::eGetId_Canonical);
                if (idh) {
                    query = idh;
                }
            }
            if (m_QueryBlacklist.size()  &&
                m_QueryBlacklist.find(query) != m_QueryBlacklist.end()) {
                /// reject: query sequence found in black list
                return false;
            }

            if (m_QueryWhitelist.size()  &&
                m_QueryWhitelist.find(query) != m_QueryWhitelist.end()) {
                /// accept: query sequence found in white list
                x_UpdateDictionaryStates(align);
                return true;
            }
        }

        if (m_SubjectBlacklist.size()  ||  m_SubjectWhitelist.size()) {
            CSeq_id_Handle subject =
                CSeq_id_Handle::GetHandle(align.GetSeq_id(1));
            if (m_Scope) {
                CSeq_id_Handle idh =
                    sequence::GetId(subject, *m_Scope,
                                    sequence::eGetId_Canonical);
                if (idh) {
                    subject = idh;
                }
            }
            if (m_SubjectBlacklist.size()  &&
                m_SubjectBlacklist.find(subject) != m_SubjectBlacklist.end()) {
                /// reject: subject sequence found in black list
                return false;
            }

            if (m_SubjectWhitelist.size()  &&
                m_SubjectWhitelist.find(subject) != m_SubjectWhitelist.end()) {
                /// accept: subject sequence found in white list
                x_UpdateDictionaryStates(align);
                return true;
            }
        }
    }

    /// check to see if scores match
    bool match = true;
    if (m_ParseTree.get()) {
        match = x_Match(*m_ParseTree->GetQueryTree(), align);
        if (match) {
            x_UpdateDictionaryStates(align);
        }
    } else {
        if (m_QueryWhitelist.size()  ||  m_SubjectWhitelist.size()) {
            /// the user supplied inclusion criteria but no filter
            /// inclusion failed - return false
            return false;
        }
        else if (m_QueryBlacklist.size()  ||  m_SubjectBlacklist.size()) {
            /// the user supplied exclusion criteria but no filter
            /// exclusion failed - return true
            return true;
        }
    }

    return (match  &&  ( !m_RemoveDuplicates  ||  x_IsUnique(align) ) );
}

void CAlignFilter::x_UpdateDictionaryStates(const objects::CSeq_align& align)
{
    if (m_ParseTree.get()) {
        NON_CONST_ITERATE (TScoreDictionary, it, m_Scores) {
            it->second->UpdateState(align);
        }
    }
}

void CAlignFilter::PrintDictionary(CNcbiOstream &ostr)
{
    ITERATE (TScoreDictionary, it, m_Scores) {
        ostr << "  * " << it->first << endl;

        CNcbiOstrstream os;
        it->second->PrintHelp(os);

        string s = string(CNcbiOstrstreamToString(os));
        list<string> tmp;
        NStr::Wrap(s, 72, tmp);
        ITERATE (list<string>, i, tmp) {
            ostr << "      " << *i << endl;
        }
    }
}

void CAlignFilter::DryRun(CNcbiOstream &ostr) {
    ostr << "Parse Tree:" << endl;
    m_ParseTree->Print(ostr);
    ostr << endl;

    m_IsDryRun = true;
    m_DryRunOutput = &ostr;
    CSeq_align dummy_alignment;
    x_Match(*m_ParseTree->GetQueryTree(), dummy_alignment);
}

bool CAlignFilter::x_IsUnique(const CSeq_align& align)
{
    CChecksumStreamWriter md5(CChecksum::eMD5);
    {{
        CWStream wstr(&md5);
        wstr << MSerial_AsnBinary << align;
     }}

    string md5_str;
    md5.GetChecksum().GetMD5Digest(md5_str);
    return m_UniqueAligns.insert(md5_str).second;
}

double CAlignFilter::x_GetAlignmentScore(const string& score_name,
                                         const objects::CSeq_align& align,
                                         bool throw_if_not_found)
{
    ///
    /// see if we have this score
    ///
    double score_value = numeric_limits<double>::quiet_NaN();

    TScoreDictionary::const_iterator it = m_Scores.find(score_name);
    if (m_IsDryRun) {
        (*m_DryRunOutput) << score_name << ": ";
        if (it != m_Scores.end()) {
            it->second->PrintHelp(*m_DryRunOutput);
        } else {
            (*m_DryRunOutput) << "assumed to be a score on the Seq-align";
        }
        (*m_DryRunOutput) << endl;
        return score_value;
    }

    if (it != m_Scores.end()) {
        return it->second->Get(align, m_Scope);
    }
    else {
        bool found = false;
        if (align.IsSetScore()) {
            ITERATE (CSeq_align::TScore, iter, align.GetScore()) {
                const CScore& score = **iter;
                if ( !score.IsSetId()  ||
                     !score.GetId().IsStr() ) {
                    continue;
                }

                if (score.GetId().GetStr() != score_name) {
                    continue;
                }

                if (score.GetValue().IsInt()) {
                    score_value = score.GetValue().GetInt();
                } else {
                    score_value = score.GetValue().GetReal();
                }

                found = true;
                break;
            }
        }
        if ( !found ) {
            if( throw_if_not_found ) {
                NCBI_THROW(CException, eUnknown,
                           "failed to find score: " + score_name);
            } else {
                LOG_POST(Warning << "failed to find score: " << score_name);
            }
        }
    }
    return score_value;
}

double CAlignFilter::x_FuncCall(const CQueryParseTree::TNode& node, const CSeq_align& align)
{
    double this_val = numeric_limits<double>::quiet_NaN();
    string function = node.GetValue().GetStrValue();
    if (NStr::EqualNocase(function, "MUL")) {
        CQueryParseTree::TNode::TNodeList_CI iter =
            node.SubNodeBegin();
        CQueryParseTree::TNode::TNodeList_CI end =
            node.SubNodeEnd();
        const CQueryParseTree::TNode& node1 = **iter;
        ++iter;
        if (iter == end) {
            NCBI_THROW(CException, eUnknown,
                       "invalid number of nodes: expected 2, got 1");
        }
        const CQueryParseTree::TNode& node2 = **iter;
        ++iter;
        if (iter != end) {
            NCBI_THROW(CException, eUnknown,
                       "invalid number of nodes: "
                       "expected 2, got more than 2");
        }

        double val1 = x_TermValue(node1, align);
        double val2 = x_TermValue(node2, align);

        this_val = val1 * val2;

    }
    else if (NStr::EqualNocase(function, "DIV")) {
        CQueryParseTree::TNode::TNodeList_CI iter =
            node.SubNodeBegin();
        CQueryParseTree::TNode::TNodeList_CI end =
            node.SubNodeEnd();
        const CQueryParseTree::TNode& node1 = **iter;
        ++iter;
        if (iter == end) {
            NCBI_THROW(CException, eUnknown,
                       "invalid number of nodes: expected 2, got 1");
        }
        const CQueryParseTree::TNode& node2 = **iter;
        ++iter;
        if (iter != end) {
            NCBI_THROW(CException, eUnknown,
                       "invalid number of nodes: "
                       "expected 2, got more than 2");
        }

        double val1 = x_TermValue(node1, align);
        double val2 = x_TermValue(node2, align);

        this_val = val1 / val2;
    }
    else if (NStr::EqualNocase(function, "ADD")) {
        CQueryParseTree::TNode::TNodeList_CI iter =
            node.SubNodeBegin();
        CQueryParseTree::TNode::TNodeList_CI end =
            node.SubNodeEnd();
        const CQueryParseTree::TNode& node1 = **iter;
        ++iter;
        if (iter == end) {
            NCBI_THROW(CException, eUnknown,
                       "invalid number of nodes: expected 2, got 1");
        }
        const CQueryParseTree::TNode& node2 = **iter;
        ++iter;
        if (iter != end) {
            NCBI_THROW(CException, eUnknown,
                       "invalid number of nodes: "
                       "expected 2, got more than 2");
        }

        double val1 = x_TermValue(node1, align);
        double val2 = x_TermValue(node2, align);

        this_val = val1 + val2;
    }
    else if (NStr::EqualNocase(function, "SUB")) {
        CQueryParseTree::TNode::TNodeList_CI iter =
            node.SubNodeBegin();
        CQueryParseTree::TNode::TNodeList_CI end =
            node.SubNodeEnd();
        const CQueryParseTree::TNode& node1 = **iter;
        ++iter;
        if (iter == end) {
            NCBI_THROW(CException, eUnknown,
                       "invalid number of nodes: expected 2, got 1");
        }
        const CQueryParseTree::TNode& node2 = **iter;
        ++iter;
        if (iter != end) {
            NCBI_THROW(CException, eUnknown,
                       "invalid number of nodes: "
                       "expected 2, got more than 2");
        }

        double val1 = x_TermValue(node1, align);
        double val2 = x_TermValue(node2, align);

        this_val = val1 - val2;
    }
    else if (NStr::EqualNocase(function, "COALESCE")) {
        ///
        /// standard SQL coalesce-if-null
        ///
        CQueryParseTree::TNode::TNodeList_CI iter =
            node.SubNodeBegin();
        CQueryParseTree::TNode::TNodeList_CI end =
            node.SubNodeEnd();
        for ( ;  iter != end;  ++iter) {
            try {
                this_val = x_TermValue(**iter, align, true);
                break;
            }
            catch (CException&) {
                /// In a real run, any exception we get are likely to be from terms not
                /// found, and should be ignored. In a dry run, if we get an exception it
                /// must be the result of some other problem which has to be reported
                if (m_IsDryRun) {
                    throw;
                }
            }
        }
    }
    else if (NStr::EqualNocase(function, "IS_SEG_TYPE")) {
        CQueryParseTree::TNode::TNodeList_CI iter =
            node.SubNodeBegin();
        CQueryParseTree::TNode::TNodeList_CI end =
            node.SubNodeEnd();
        const CQueryParseTree::TNode& node1 = **iter;
        ++iter;
        if (iter != end) {
            NCBI_THROW(CException, eUnknown,
                       "invalid number of nodes: expected 1, got more than 1");
        }

        /// this is a bit different - we expect a string type here
        if (node1.GetValue().GetType() != CQueryParseNode::eString) {
            NCBI_THROW(CException, eUnknown,
                       "invalid seg type - expected string");
        }
        const string& s = node1.GetValue().GetStrValue();
        if (NStr::EqualNocase(s, "denseg")) {
            if (!m_IsDryRun) {
                this_val = align.GetSegs().IsDenseg();
            }
        }
        else if (NStr::EqualNocase(s, "spliced")) {
            if (!m_IsDryRun) {
                this_val = align.GetSegs().IsSpliced();
            }
        }
        else if (NStr::EqualNocase(s, "disc")) {
            if (!m_IsDryRun) {
                this_val = align.GetSegs().IsDisc();
            }
        }
        else if (NStr::EqualNocase(s, "std")) {
            if (!m_IsDryRun) {
                this_val = align.GetSegs().IsStd();
            }
        }
        else if (NStr::EqualNocase(s, "sparse")) {
            if (!m_IsDryRun) {
                this_val = align.GetSegs().IsSparse();
            }
        }
        else if (NStr::EqualNocase(s, "dendiag")) {
            if (!m_IsDryRun) {
                this_val = align.GetSegs().IsDendiag();
            }
        }
        else if (NStr::EqualNocase(s, "packed")) {
            if (!m_IsDryRun) {
                this_val = align.GetSegs().IsPacked();
            }
        }
        else {
            NCBI_THROW(CException, eUnknown,
                       "invalid seg type: " + s);
        }
    }
    else {
        NCBI_THROW(CException, eUnknown,
                   "function not understood: " + function);
    }

    return this_val;
}


bool s_IsDouble(const string& str)
{
    ITERATE(string, iter, str) {
        if( !isdigit(*iter) &&
		    *iter != '+' &&
			*iter != '-' &&
			*iter != '.' &&
			*iter != ' ') {
            return false;
	    }
	}
	return true;
}

double CAlignFilter::x_TermValue(const CQueryParseTree::TNode& term_node,
                                 const CSeq_align& align,
                                 bool throw_if_not_found)
{
    CQueryParseNode::EType type = term_node.GetValue().GetType();
    switch (type) {
    case CQueryParseNode::eIntConst:
        return term_node.GetValue().GetInt();
    case CQueryParseNode::eFloatConst:
        return term_node.GetValue().GetDouble();
    case CQueryParseNode::eString:
        {{
             string str = term_node.GetValue().GetStrValue();
             double val;
			 if(s_IsDouble(str)) {
			     try {
			         val = NStr::StringToDouble(str);
                 }
                 catch (CException&) {
                     val = x_GetAlignmentScore(str, align, throw_if_not_found);
				 }
		     } else {
                 val = x_GetAlignmentScore(str, align, throw_if_not_found);
			 }
             return val;
         }}
    case CQueryParseNode::eFunction:
        return x_FuncCall(term_node, align);
    default:
        NCBI_THROW(CException, eUnknown,
                   "unexpected expression");
    }
}


bool CAlignFilter::x_Query_Op(const CQueryParseTree::TNode& l_node,
                              CQueryParseNode::EType type,
                              bool is_not,
                              const CQueryParseTree::TNode& r_node,
                              const CSeq_align& align)
{
    ///
    /// screen for simple sequence match first
    ///
    if (l_node.GetValue().GetType() == CQueryParseNode::eString) {
        string s = l_node.GetValue().GetStrValue();
        if (NStr::EqualNocase(s, "query")  ||
            NStr::EqualNocase(s, "subject")) {
            if (type != CQueryParseNode::eEQ) {
                NCBI_THROW(CException, eUnknown,
                           "query/subject filter is valid with equality only");
            }

            string val = r_node.GetValue().GetStrValue();
            CSeq_id_Handle idh = CSeq_id_Handle::GetHandle(val);

            if (m_IsDryRun) {
                (*m_DryRunOutput) << val << ": SeqId, ";
                CConstRef<CSynonymsSet> syns = m_Scope->GetSynonyms(idh);
                if (!syns) {
                    (*m_DryRunOutput) << "No synonyms";
                } else {
                    (*m_DryRunOutput) << "synonyms ";
                    ITERATE (CSynonymsSet::TIdSet, syn_it, *syns) {
                        if (syn_it != syns->begin()) {
                            (*m_DryRunOutput) << ",";
                        }
                        (*m_DryRunOutput) << (*syn_it)->first;
                    }
                }
                (*m_DryRunOutput) << endl;
                return false;
            } else {
                CSeq_id_Handle other_idh;
                if (NStr::EqualNocase(s, "query")) {
                    other_idh = CSeq_id_Handle::GetHandle(align.GetSeq_id(0));
                } else {
                    other_idh = CSeq_id_Handle::GetHandle(align.GetSeq_id(1));
                }
    
                if ((idh == other_idh) == !is_not) {
                    return true;
                }
    
                CConstRef<CSynonymsSet> syns = m_Scope->GetSynonyms(idh);
                if ( !syns ) {
                    return false;
                }
    
                return (syns->ContainsSynonym(other_idh) == !is_not);
            }
        }
    }

    /// fall-through: not query or subject

    double l_val = x_TermValue(l_node, align);
    double r_val = x_TermValue(r_node, align);

    if (isnan(l_val) || isnan(r_val)) {
        return false;
    }

    switch (type) {
    case CQueryParseNode::eEQ:
        return ((l_val == r_val)  ==  !is_not);

    case CQueryParseNode::eLT:
        return ((l_val <  r_val)  ==  !is_not);

    case CQueryParseNode::eLE:
        return ((l_val <= r_val)  ==  !is_not);

    case CQueryParseNode::eGT:
        return ((l_val >  r_val)  ==  !is_not);

    case CQueryParseNode::eGE:
        return ((l_val >= r_val)  ==  !is_not);

    default:
        LOG_POST(Warning << "unhandled parse node in expression");
        break;
    }

    return false;
}


bool CAlignFilter::x_Query_Range(const CQueryParseTree::TNode& key_node,
                                 bool is_not,
                                 const CQueryParseTree::TNode& val1_node,
                                 const CQueryParseTree::TNode& val2_node,
                                 const CSeq_align& align)
{
    double this_val = x_TermValue(key_node, align);
    double val1 = x_TermValue(val1_node, align);
    double val2 = x_TermValue(val2_node, align);
    if (val1 > val2) {
        swap(val1, val2);
    }

    if ( !isnan(this_val)  &&  !isnan(val1)  &&  !isnan(val2)) {
        bool between = (val1 <= this_val)  &&  (this_val <= val2);
        return (between == !is_not);
    }

    return false;
}


bool CAlignFilter::x_Match(const CQueryParseTree::TNode& node,
                           const CSeq_align& align)
{
    switch (node.GetValue().GetType()) {
    case CQueryParseNode::eEQ:
    case CQueryParseNode::eGT:
    case CQueryParseNode::eGE:
    case CQueryParseNode::eLT:
    case CQueryParseNode::eLE:
        {{
             CQueryParseTree::TNode::TNodeList_CI iter =
                 node.SubNodeBegin();
             CQueryParseTree::TNode::TNodeList_CI end =
                 node.SubNodeEnd();
             const CQueryParseTree::TNode& node1 = **iter;
             ++iter;
             if (iter == end) {
                 NCBI_THROW(CException, eUnknown,
                            "invalid number of nodes: expected 2, got 1");
             }
             const CQueryParseTree::TNode& node2 = **iter;
             ++iter;
             if (iter != end) {
                 NCBI_THROW(CException, eUnknown,
                            "invalid number of nodes: "
                            "expected 2, got more than 2");
             }

             return x_Query_Op(node1,
                               node.GetValue().GetType(),
                               node.GetValue().IsNot(),
                               node2, align);
         }}
        break;

    case CQueryParseNode::eBetween:
        {{
             CQueryParseTree::TNode::TNodeList_CI iter =
                 node.SubNodeBegin();
             CQueryParseTree::TNode::TNodeList_CI end =
                 node.SubNodeEnd();
             const CQueryParseTree::TNode& node1 = **iter;
             ++iter;
             if (iter == end) {
                 NCBI_THROW(CException, eUnknown,
                            "invalid number of nodes: expected 3, got 1");
             }
             const CQueryParseTree::TNode& node2 = **iter;
             ++iter;
             if (iter == end) {
                 NCBI_THROW(CException, eUnknown,
                            "invalid number of nodes: "
                            "expected 3, got 2");
             }

             const CQueryParseTree::TNode& node3 = **iter;
             ++iter;
             if (iter != end) {
                 NCBI_THROW(CException, eUnknown,
                            "invalid number of nodes: "
                            "expected 3, got more than 3");
             }

             if (node1.GetValue().GetType() != CQueryParseNode::eString) {
                 NCBI_THROW(CException, eUnknown,
                            "unexpected expression");
             }
             return x_Query_Range(node1,
                                  node.GetValue().IsNot(),
                                  node2, node3, align);
         }}
        break;

    case CQueryParseNode::eAnd:
        {{
             bool res = false;
             CQueryParseTree::TNode::TNodeList_CI iter =
                 node.SubNodeBegin();
             if (iter != node.SubNodeEnd()) {
                 res = x_Match(**iter, align);
                 ++iter;
             }
             /// In a real run, use short-circuit logic; in a dry run, always interpret
             /// both sides
             for ( ;  iter != node.SubNodeEnd()  &&
                      (res || m_IsDryRun);
                      ++iter)
             {
                 res &= x_Match(**iter, align);
             }

             return res;
         }}

    case CQueryParseNode::eNot:
        {{
             bool res = false;
             CQueryParseTree::TNode::TNodeList_CI iter =
                 node.SubNodeBegin();
             if (iter != node.SubNodeEnd()) {
                 res = !x_Match(**iter, align);
             }

             return res;
         }}

    case CQueryParseNode::eOr:
        {{
             bool res = false;
             CQueryParseTree::TNode::TNodeList_CI iter =
                 node.SubNodeBegin();
             if (iter != node.SubNodeEnd()) {
                 res = x_Match(**iter, align);
                 ++iter;
             }
             /// In a real run, use short-circuit logic; in a dry run, always interpret
             /// both sides
             for ( ;  iter != node.SubNodeEnd()  &&
                      (!res || m_IsDryRun);
                      ++iter)
             {
                 res |= x_Match(**iter, align);
             }

             return res;
         }}


    default:
        LOG_POST(Warning << "unhandled parse node in tree");
        break;
    }

    return false;
}



END_NCBI_SCOPE

