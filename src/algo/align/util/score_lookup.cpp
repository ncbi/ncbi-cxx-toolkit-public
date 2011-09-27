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
 * Authors:  Eyal Mozes
 *
 * File Description:
 *
 */

#include <ncbi_pch.hpp>
#include <algo/sequence/gene_model.hpp>
#include <algo/align/util/algo_align_util_exceptions.hpp>
#include <algo/align/util/score_lookup.hpp>

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

#include <objmgr/object_manager.hpp>
#include <objmgr/scope.hpp>
#include <objmgr/util/sequence.hpp>

BEGIN_NCBI_SCOPE
USING_SCOPE(objects);


/////////////////////////////////////////////////////////////////////////////

class CScore_AlignLength : public CScoreLookup::IScore
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

class CScore_LongestGapLength : public CScoreLookup::IScore
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

class CScore_3PrimeUnaligned : public CScoreLookup::IScore
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

class CScore_InternalUnaligned : public CScoreLookup::IScore
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

class CScore_AlignStartStop : public CScoreLookup::IScore
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

class CScore_AlignLengthRatio : public CScoreLookup::IScore
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

class CScore_SequenceLength : public CScoreLookup::IScore
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

class CScore_SymmetricOverlap : public CScoreLookup::IScore
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

class CScore_MinExonLength : public CScoreLookup::IScore
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

class CScore_MaxIntronLength : public CScoreLookup::IScore
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

class CScore_ExonCount : public CScoreLookup::IScore
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

class CScore_CdsInternalStops : public CScoreLookup::IScore
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

class CScore_CdsScore : public CScoreLookup::IScore
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

class CScore_Taxid : public CScoreLookup::IScore
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

class CScore_LastSpliceSite : public CScoreLookup::IScore
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

class CScore_Overlap : public CScoreLookup::IScore
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


void CScoreLookup::x_Init()
{
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


void CScoreLookup::UpdateState(const objects::CSeq_align& align)
{
    ITERATE (set<string>, it, m_ScoresUsed) {
        m_Scores[*it]->UpdateState(align);
    }
}

void CScoreLookup::x_PrintDictionaryEntry(CNcbiOstream &ostr,
                                          const string &score_name)
{
    ostr << "  * " << score_name << endl;

    list<string> tmp;
    NStr::Wrap(HelpText(score_name), 72, tmp);
    ITERATE (list<string>, i, tmp) {
        ostr << "      " << *i << endl;
    }
}

void CScoreLookup::PrintDictionary(CNcbiOstream &ostr)
{
    ostr << "Build-in score names: " << endl;
    ITERATE (CSeq_align::TScoreNameMap, it, CSeq_align::ScoreNameMap()) {
        x_PrintDictionaryEntry(ostr, it->first);
    }
    ostr << endl;

    ostr << "Computed tokens: " << endl;
    ITERATE (TScoreDictionary, it, m_Scores) {
        x_PrintDictionaryEntry(ostr, it->first);
    }
}

string CScoreLookup::HelpText(const string &score_name)
{
    TScoreDictionary::const_iterator it = m_Scores.find(score_name);
    if (it != m_Scores.end()) {
        m_ScoresUsed.insert(score_name);
        CNcbiOstrstream os;
        it->second->PrintHelp(os);
        return string(CNcbiOstrstreamToString(os));
    } else {
        return CSeq_align::HelpText(score_name);
    }
}

double CScoreLookup::GetScore(const objects::CSeq_align& align,
                              const string &score_name)
{
    double score;
    if (align.GetNamedScore(score_name, score)) {
        return score;
    }

    /// Score not found in alignmnet; look for it among built-in scores
    CSeq_align::TScoreNameMap::const_iterator score_it =
        CSeq_align::ScoreNameMap().find(score_name);
    if (score_it != CSeq_align::ScoreNameMap().end()) {
        return m_ScoreBuilder.ComputeScore(*m_Scope, align, score_it->second);
    }

    /// Not a built-in score; look for it among computed tokens
    TScoreDictionary::const_iterator token_it = m_Scores.find(score_name);
    if (token_it != m_Scores.end()) {
        return token_it->second->Get(align, &*m_Scope);
    }

    NCBI_THROW(CAlgoAlignUtilException, eScoreNotFound, score_name);
}

END_NCBI_SCOPE

