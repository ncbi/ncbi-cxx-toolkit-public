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
#include <algo/sequence/internal_stops.hpp>
#include <algo/align/util/algo_align_util_exceptions.hpp>
#include <algo/align/util/score_lookup.hpp>

#include <objects/seqalign/Seq_align_set.hpp>
#include <objects/seqalign/Seq_align.hpp>
#include <objects/seqalign/Dense_seg.hpp>
#include <objects/seqalign/Score.hpp>
#include <objects/seqalign/Score_set.hpp>
#include <objects/seqalign/Spliced_seg.hpp>
#include <objects/seqalign/Spliced_seg_modifier.hpp>
#include <objects/seqalign/Spliced_exon.hpp>
#include <objects/seqalign/Spliced_exon_chunk.hpp>
#include <objects/seqalign/Product_pos.hpp>
#include <objects/seqalign/Prot_pos.hpp>
#include <objects/seqfeat/Org_ref.hpp>
#include <objects/seqfeat/Genetic_code_table.hpp>
#include <objects/seqfeat/Genetic_code.hpp>
#include <objects/seqfeat/Cdregion.hpp>
#include <objects/general/Object_id.hpp>
#include <objects/general/Dbtag.hpp>
#include <objects/seq/Seq_annot.hpp>

#include <objmgr/object_manager.hpp>
#include <objmgr/scope.hpp>
#include <objmgr/seq_vector.hpp>
#include <objmgr/util/sequence.hpp>

#include <objtools/alnmgr/alntext.hpp>
#include <objtools/alnmgr/pairwise_aln.hpp>
#include <objtools/alnmgr/aln_converters.hpp>

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

    virtual bool IsInteger() const { return true; };

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

class CScore_GapCount : public CScoreLookup::IScore
{
public:
    CScore_GapCount(bool count_bases, int row = -1,
                    bool exon_specific = false)
        : m_CountBases(count_bases), m_Row(row), m_ExonSpecific(exon_specific)
    {
    }

    virtual EComplexity GetComplexity() const { return eEasy; };

    virtual bool IsInteger() const { return true; };

    virtual double Get(const CSeq_align& align, CScope*) const
    {
        if (m_ExonSpecific && !align.GetSegs().IsSpliced()) {
            NCBI_THROW(CSeqalignException, eUnsupported,
                       "'product_gap_length' and 'genomic_gap_length' scores "
                       "valid only for Spliced-seg alignments");
        }
        return m_CountBases ? align.GetTotalGapCount(m_Row)
                            : align.GetNumGapOpenings(m_Row);
    }

    virtual void PrintHelp(CNcbiOstream& ostr) const
    {
        if (m_CountBases) {
            ostr << "Total number of gap bases";
        }
        else {
            ostr << "Number of gap openings";
        }
        if (m_ExonSpecific) {
            if (m_Row == 0) {
                ostr << " in product exons";
            } else if(m_Row == 1) {
                ostr << " in genomic exons";
            }
        } else {
            if (m_Row == 0) {
                ostr << " in query";
            } else if(m_Row == 1) {
                ostr << " in subject";
            }
        }
    }

private:
    bool m_CountBases;
    int m_Row;
    bool m_ExonSpecific;
};

/////////////////////////////////////////////////////////////////////////////

class CScore_FrameShifts : public CScoreLookup::IScore
{
public:
    CScore_FrameShifts(int row = -1)
        : m_Row(row)
    {
    }

    virtual EComplexity GetComplexity() const { return eEasy; };

    virtual bool IsInteger() const { return true; };

    virtual double Get(const CSeq_align& align, CScope*) const
    {
        return align.GetNumFrameshifts(m_Row);
    }

    virtual void PrintHelp(CNcbiOstream& ostr) const
    {
        ostr << "Number of frame shifts";
        if (m_Row == 0) {
            ostr << " in query";
        } else if(m_Row == 1) {
            ostr << " in subject";
        }
    }

private:
    int m_Row;
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

    virtual bool IsInteger() const { return true; };

    virtual double Get(const CSeq_align& align, CScope* s) const
    {
        return align.GapLengthRange().second;
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

    virtual bool IsInteger() const { return true; };

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

    virtual bool IsInteger() const { return true; };

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
                             TSeqPos curr_nuc = (*it)->GetProduct_start().AsSeqPos();
                             TSeqPos last_nuc = (*prev)->GetProduct_end().AsSeqPos();
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
                             TSeqPos curr_nuc = (*it)->GetProduct_start().AsSeqPos();
                             TSeqPos last_nuc = (*prev)->GetProduct_end().AsSeqPos();
                             score_value += curr_nuc - last_nuc - 1;
                         }
                     }
                 }
             }}
            break;

        default:
            NCBI_THROW(CSeqalignException, eNotImplemented,
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

    virtual bool IsInteger() const { return true; };

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

    virtual bool IsInteger() const { return true; };

    virtual double Get(const CSeq_align& align, CScope* scope) const
    {
        if (m_Row == 0  &&  align.GetSegs().IsSpliced()) {
            return align.GetSegs().GetSpliced().GetProduct_length();
        } else {
            if (scope) {
                CBioseq_Handle bsh =
                    scope->GetBioseqHandle(align.GetSeq_id(m_Row));
                if (bsh) {
                    return bsh.GetBioseqLength();
                } else {
                    NCBI_THROW(CSeqalignException, eInvalidSeqId,
                               "Can't get length for sequence " +
                               align.GetSeq_id(m_Row).AsFastaString());
                }
            }
        }
        return 0;
    }

private:
    int m_Row;
};

//////////////////////////////////////////////////////////////////////////////
/// Get sequence's length in nucleic acids
static inline TSeqPos s_GetNaLength(CBioseq_Handle bsh)
{
    TSeqPos len = bsh.GetBioseqLength();
    if (bsh.CanGetInst_Mol() && bsh.GetInst_Mol() == CSeq_inst::eMol_aa) {
        /// This is an amino-acid sequence, so multiply length by 3
        len *= 3;
    }
    return len;
}


class CScore_SymmetricOverlap : public CScoreLookup::IScore
{
public:
    enum EType {e_Min, e_Avg};
    CScore_SymmetricOverlap(EType type)
    : m_Type(type)
    {
    }

    virtual void PrintHelp(CNcbiOstream& ostr) const
    {
        ostr <<
            "Symmetric overlap, as a percent (0-100).  This is similar to "
            "coverage, except that it takes into account both query and "
            "subject sequence lengths. Alignment length is divided by "
             << (m_Type == e_Min ? "minimum" : "average")
             << " of the two sequence lengths";
    }

    virtual EComplexity GetComplexity() const { return eHard; };

    virtual double Get(const CSeq_align& align, CScope* scope) const
    {
        TSeqPos length = align.GetAlignLength(false);
        double pct_overlap = length * 100;

        CBioseq_Handle q = scope->GetBioseqHandle(align.GetSeq_id(0));
        CBioseq_Handle s = scope->GetBioseqHandle(align.GetSeq_id(1));
        if (q.IsAa()  &&  s.IsAa()) {
            pct_overlap *= 3;
        }

        switch (m_Type) {
            case e_Min:
                pct_overlap /= min(s_GetNaLength(q), s_GetNaLength(s));
                break;

            case e_Avg:
                pct_overlap /= (s_GetNaLength(q) + s_GetNaLength(s))/2;
                break;
        }
        return pct_overlap;
    }

private:
    EType m_Type;
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

    virtual bool IsInteger() const { return true; };

    virtual double Get(const CSeq_align& align, CScope*) const
    {
        return align.ExonLengthRange().first;
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

    virtual bool IsInteger() const { return true; };

    virtual double Get(const CSeq_align& align, CScope*) const
    {
        return align.IntronLengthRange().second;
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

    virtual bool IsInteger() const { return true; };

    virtual double Get(const CSeq_align& align, CScope*) const
    {
        if (align.GetSegs().IsSpliced()) {
            const CSpliced_seg& seg = align.GetSegs().GetSpliced();
            if (seg.IsSetExons()) {
                return seg.GetExons().size();
            }
            return 0;
        }

        NCBI_THROW(CSeqalignException, eUnsupported,
                   "'exon_count' score is valid only for "
                   "Spliced-seg alignments");
    }
};

//////////////////////////////////////////////////////////////////////////////

static const CGenetic_code *s_GetGeneticCode(const CSeq_id& seq_id,
                                             CScope* scope)
{
    CRef<CGenetic_code> genetic_code;
    try {
        CBioseq_Handle bsh = scope->GetBioseqHandle(seq_id);
        int gcode = sequence::GetOrg_ref(bsh).GetGcode();
        const CGenetic_code_table& tbl = CGen_code_table::GetCodeTable();
        ITERATE (CGenetic_code_table::Tdata, it, tbl.Get()) {
            if ((*it)->GetId() == gcode) {
                genetic_code = *it;
                break;
            }
        }
    }
    catch (CException&) {
        // use the default genetic code
    }

    return genetic_code.GetPointer();
}

class CScore_StartStopCodon : public CScoreLookup::IScore
{
public:
    CScore_StartStopCodon(bool start_codon)
    : m_StartCodon(start_codon)
    {
    }

    virtual void PrintHelp(CNcbiOstream& ostr) const
    {
        ostr << "1 if a " << (m_StartCodon ? "start" : "stop")
             << " codon was found, 0 otherwise. Note that this score has "
              "meaning only for Spliced-seg alignments, as would be generated "
              "by Splign or ProSplign.";
    }

    virtual EComplexity GetComplexity() const { return eEasy; };

    virtual bool IsInteger() const { return true; };

    virtual double Get(const CSeq_align& align, CScope* scope) const
    {
        bool is_protein = false;
        TSeqPos product_length = 0;
        if (align.GetSegs().IsSpliced()) {
            bool score_precalculated=false;
            const CSpliced_seg& seg = align.GetSegs().GetSpliced();
            is_protein = seg.GetProduct_type() ==
                CSpliced_seg::eProduct_type_protein;
            if (seg.CanGetProduct_length()) {
                product_length = seg.GetProduct_length();
            }
            ITERATE (CSpliced_seg::TModifiers, it, seg.GetModifiers()) {
                if (m_StartCodon
                    ? (*it)->IsStart_codon_found() 
                    : (*it)->IsStop_codon_found() ) {
                    score_precalculated=true;
                    if (m_StartCodon
                        ? (*it)->GetStart_codon_found()
                        : (*it)->GetStop_codon_found())
                    {
                        return 1;
                    }
                }
            }
            if (score_precalculated) {
                /// Found the modifier, but it was set to false
                return 0;
            }
        }

        if (!product_length) {
            CBioseq_Handle product_bsh =
                scope->GetBioseqHandle(align.GetSeq_id(0));
            if (!product_bsh) {
                NCBI_THROW(CSeqalignException, eUnsupported,
                           "Can't get sequence " +
                           align.GetSeq_id(0).AsFastaString());
            }
            is_protein = product_bsh.IsAa();
            product_length = product_bsh.GetBioseqLength();
        }

        CRef<CSeq_loc> aligned_genomic;

        //
        // generate the cleaned alignment
        //

        CFeatureGenerator generator(*scope);
        generator.SetAllowedUnaligned(10);
        CConstRef<CSeq_align> clean_align = generator.CleanAlignment(align);

        // we can't call CFeatureGenerator because CFeatureGenerator depends on
        // having certain fields set (such as Spliced-seg modifiers indicating
        // (wait for it...) that the stop codon or start codon was found.  This
        // here function is to be called to verify that the star/stop are
        // included, hence we have a circular logical relationship...
        CSeq_id &query_id = const_cast<CSeq_id &>(clean_align->GetSeq_id(0));
        CSeq_id &subject_id = const_cast<CSeq_id &>(clean_align->GetSeq_id(1));
        CBioseq_Handle genomic_bsh = scope->GetBioseqHandle(subject_id);
        TSeqPos genomic_len = genomic_bsh.GetBioseqLength();

        CSeq_loc_Mapper mapper(*clean_align, 1);

        CRef<CSeq_loc> cds_loc;
        if (is_protein) {
            CSeq_loc loc;
            loc.SetWhole().Assign(query_id);
            cds_loc = mapper.Map(loc);
        }
        else {
            CBioseq_Handle bsh = scope->GetBioseqHandle(query_id);
            CFeat_CI feat_it(bsh,
                             SAnnotSelector()
                             .IncludeFeatType(CSeqFeatData::e_Cdregion));

            CMappedFeat mf;
            for ( ;  feat_it;  ++feat_it) {
                mf = *feat_it;
                break;
            }

            if ( !mf ) {
                // no CDS == no start or stop
                return 0.0;
            }

            const CSeq_loc &orig_loc = mf.GetLocation();
            ENa_strand q_strand = sequence::GetStrand(orig_loc, scope);
            TSeqRange total_q_range = orig_loc.GetTotalRange();
            if (!orig_loc.IsPartialStop(eExtreme_Biological)) {
                /// Remove stop codon
                if (q_strand == eNa_strand_minus) {
                    total_q_range.SetFrom(total_q_range.GetFrom() + 3);
                }
                else {
                    total_q_range.SetTo(total_q_range.GetTo() - 3);
                }
            }

            /**
            cerr << "orig loc: " << MSerial_AsnText << orig_loc;
            cerr << "orig strand: " << s_strand << endl;
            cerr << "orig range: " << total_s_range << endl;
            **/

            if (mf.GetData().GetCdregion().IsSetFrame() &&
                mf.GetData().GetCdregion().GetFrame() > 1)
            {
                TSeqPos offs = mf.GetData().GetCdregion().GetFrame() - 1;
                if (q_strand == eNa_strand_minus) {
                    total_q_range.SetTo(total_q_range.GetTo() + offs);
                }
                else {
                    total_q_range.SetFrom(total_q_range.GetFrom() - offs);
                }
            }
            CSeq_loc adjusted_loc(
                query_id, total_q_range.GetFrom(),
                total_q_range.GetTo(), q_strand);

            // map the mRNA locations to the genome
            cds_loc = mapper.Map(adjusted_loc);

            /**
            if (start_codon) {
                cerr << "start codon: " << MSerial_AsnText << *start_codon;
            }
            if (stop_codon) {
                cerr << "stop codon: " << MSerial_AsnText << *stop_codon;
            }
            **/
        }

        ENa_strand s_strand = sequence::GetStrand(*cds_loc, scope);
        int direction = s_strand == eNa_strand_minus ? -1 : 1;
        int from =
            m_StartCodon ? (int)cds_loc->GetStart(eExtreme_Biological)
                      : (int)cds_loc->GetStop(eExtreme_Biological) + direction;
        int to = from + 2 * direction;
        CRef<CSeq_loc> codon;
        if (to >= 0 && to < genomic_len) {
            /// codon is simple interval
            codon.Reset(new CSeq_loc(subject_id, min(from,to), max(from,to),
                                     s_strand));
        } else if (genomic_bsh.GetInst_Topology() ==
                   CSeq_inst::eTopology_circular)
        {
            /// this is a circular genomic sequence, and codon crosses origin
            CRef<CSeq_interval> int1, int2;
            if (s_strand == eNa_strand_minus) {
                int1.Reset(new CSeq_interval(subject_id, 0, from,
                                             eNa_strand_minus));
                int1->SetFuzz_from().SetLim(CInt_fuzz::eLim_circle);
                int2.Reset(new CSeq_interval(subject_id, to + genomic_len,
                                         genomic_len - 1, eNa_strand_minus));
                int2->SetFuzz_to().SetLim(CInt_fuzz::eLim_circle);
            } else {
                int1.Reset(new CSeq_interval(subject_id, from,
                                         genomic_len - 1, eNa_strand_plus));
                int1->SetFuzz_to().SetLim(CInt_fuzz::eLim_circle);
                int2.Reset(new CSeq_interval(subject_id, 0, to - genomic_len,
                                             eNa_strand_plus));
                int2->SetFuzz_from().SetLim(CInt_fuzz::eLim_circle);
            }
            codon.Reset(new CSeq_loc);
            codon->SetPacked_int().Set().push_back(int1);
            codon->SetPacked_int().Set().push_back(int2);
        }

        if ( !codon ) {
            return 0.0;
        }

        //
        // evaluate for start-stop codon as needed
        //

        int gcode = 11;
        const CGenetic_code* gc = s_GetGeneticCode(align.GetSeq_id(1), scope);
        if (gc) {
            gcode = gc->GetId();
        }
        const CTrans_table& tbl = CGen_code_table::GetTransTable(gcode);

        CSeqVector v(*codon, scope, CBioseq_Handle::eCoding_Iupac);

        /**
        cerr << MSerial_AsnText << *start_codon;
        cerr << "gcode: " << gcode << endl;
        cerr << "bases: "
            << v[0] << v[1] << v[2] << endl;
            **/

        int state = tbl.SetCodonState(v[0], v[1], v[2]);
        if (m_StartCodon ? tbl.IsAnyStart(state) : tbl.IsOrfStop(state)) {
            return 1.0;
        }

        return 0.0;
    }


private:
    bool m_StartCodon;
};

//////////////////////////////////////////////////////////////////////////////

class CScore_CdsInternalStops : public CScoreLookup::IScore
{
public:
    virtual void PrintHelp(CNcbiOstream& ostr) const
    {
        ostr <<
            "Count of the number of internal stop codons encountered when "
            "translating the aligned coding region. Note that this has meaning "
            "only for Spliced-seg transcript alignments with a transcript that "
            "has an annotated cdregion, or for Spliced-seg protein alignments.";
    }

    virtual EComplexity GetComplexity() const { return eHard; };

    virtual bool IsInteger() const { return true; };

    virtual double Get(const CSeq_align& align, CScope* scope) const
    {

        if (align.GetSegs().IsSpliced()) {
            CInternalStopFinder stop_finder(*scope);
            return stop_finder.FindStops(align).size();
        }

        double score = 0;

        //
        // complicated
        //

        // first, generate a gene model
        CFeatureGenerator generator(*scope);
        generator.SetFlags(CFeatureGenerator::fForceTranscribeMrna |
                           CFeatureGenerator::fCreateCdregion);
        generator.SetAllowedUnaligned(10);

        CConstRef<CSeq_align> clean_align = generator.CleanAlignment(align);
        CSeq_annot annot;
        CBioseq_set bset;
        generator.ConvertAlignToAnnot(*clean_align, annot, bset);
        if (bset.GetSeq_set().empty() ||
            !bset.GetSeq_set().front()->IsSetAnnot())
        {
            return score;
        }

        CScope transcribed_mrna_scope(*CObjectManager::GetInstance());
        transcribed_mrna_scope.AddTopLevelSeqEntry(*bset.GetSeq_set().front());
        CRef<CSeq_feat> cds = bset.GetSeq_set().front()
                                  -> GetSeq().GetAnnot().front()
                                  -> GetData().GetFtable().front();

        if (cds) {
            cds->SetData().SetCdregion().ResetCode_break();
            string trans;
	    CSeqTranslator::Translate(*cds, transcribed_mrna_scope, trans);
	    if ( !cds->GetLocation().IsPartialStop(eExtreme_Biological)  &&
                 NStr::EndsWith(trans, "*"))
            {
                trans.resize(trans.size() - 1);
            }

            ITERATE (string, i, trans) {
                score += (*i == '*');
            }

            /**
            cerr << "align: "
                << CSeq_id_Handle::GetHandle(align.GetSeq_id(0))
                << " x "
                << CSeq_id_Handle::GetHandle(align.GetSeq_id(1))
                << endl;

            if (cds->IsSetProduct()) {
                string seq;
                CSeqVector v(cds->GetProduct(), *scope, CBioseq_Handle::eCoding_Iupac);
                v.GetSeqData(v.begin(), v.end(), seq);
                cerr << "product: " << seq << endl;
            }
            cerr << "xlate:   " << trans << endl;
            cerr << "count: " << score << endl;
            **/
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

    virtual bool IsInteger() const { return m_ScoreType >= eStart; };

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

    virtual bool IsInteger() const { return true; };

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
        return scope->GetTaxId(align.GetSeq_id(m_Row));

        /**
        return sequence::GetTaxId
            (scope->GetBioseqHandle(align.GetSeq_id(m_Row)));
            **/
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

    virtual bool IsInteger() const { return true; };

    virtual double Get(const CSeq_align& align, CScope* scope) const
    {
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
                    return last_spliced_exon.GetProduct_end().GetNucpos();
                }
            }
        }
        NCBI_THROW(CSeqalignException, eUnsupported,
                   "last_splice_site score inapplicable");
        return 0;
    }
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

    virtual bool IsInteger() const { return true; };

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


//////////////////////////////////////////////////////////////////////////////

class CScore_TblastnScore : public CScoreLookup::IScore
{
public:
    CScore_TblastnScore(const string& matrix)
        : m_Matrix(matrix)
    {
    }

    virtual void PrintHelp(CNcbiOstream& ostr) const
    {
        ostr <<
            "Recompute a raw BLAST score for an arbitrary protein-to-DNA "
            "alignment, using a Spliced-seg as input.  Computation is "
            "constrained to accept only protein-to-nucleotide Spliced-seg "
            "alignments and is slightly different than the raw BLAST score, "
            "in that gap computations differ due to the lack of true "
            "composition based statistics.  These differences are minimal.";
    }

    virtual EComplexity GetComplexity() const { return eHard; };

    virtual bool IsInteger() const { return true; };

    virtual double Get(const CSeq_align& align, CScope* scope) const
    {
        // check assumptions:
        //
        if ( !align.GetSegs().IsSpliced() ) {
            NCBI_THROW(CSeqalignException, eUnsupported,
                       "CScore_TblastnScore: "
                       "valid only for spliced-seg alignments");
        }

        if ( align.GetSegs().GetSpliced().GetProduct_type() !=
             CSpliced_seg::eProduct_type_protein) {
            NCBI_THROW(CSeqalignException, eUnsupported,
                       "CScore_TblastnScore: "
                       "valid only for protein spliced-seg alignments");
        }

        CSeq_id_Handle prot_idh = CSeq_id_Handle::GetHandle(align.GetSeq_id(0));
        CSeq_id_Handle genomic_idh =
            CSeq_id_Handle::GetHandle(align.GetSeq_id(1));

        ENa_strand strand = align.GetSeqStrand(1);
        CBioseq_Handle prot_bsh = scope->GetBioseqHandle(prot_idh);
        CBioseq_Handle genomic_bsh = scope->GetBioseqHandle(genomic_idh);
        CSeqVector prot_vec   (prot_bsh,    CBioseq_Handle::eCoding_Iupac);

        int gcode = 1;
        try {
            gcode = sequence::GetOrg_ref(genomic_bsh).GetGcode();
        }
        catch (CException&) {
            // use the default genetic code
        }

        const CTrans_table& tbl = CGen_code_table::GetTransTable(gcode);

        int state = 0;
        int offs = 0;
        int score = 0;

        const SNCBIPackedScoreMatrix* packed_mtx =
            NCBISM_GetStandardMatrix(m_Matrix.c_str());
        if (packed_mtx == NULL) {
            NCBI_THROW(CException, eUnknown,
                       "unknown scoring matrix: " + m_Matrix);
        }
        SNCBIFullScoreMatrix full_matrix;
        NCBISM_Unpack(packed_mtx, &full_matrix);

        //LOG_POST(Error << "align: " << prot_idh << " x " << genomic_idh);

        CSeq_align sub_align;
        sub_align.Assign(align);

        int gap_count = 0;
        int gap_bases = 0;
        int num_positives = 0;
        int num_negatives = 0;
        int num_match = 0;
        int num_mismatch = 0;
        ITERATE (CSpliced_seg::TExons, it,
                 align.GetSegs().GetSpliced().GetExons()) {
            CRef<CSpliced_exon> exon = *it;
            sub_align.SetSegs().SetSpliced().SetExons().clear();
            sub_align.SetSegs().SetSpliced().SetExons().push_back(exon);

            CRef<CPairwiseAln> aln = CreatePairwiseAlnFromSeqAlign(sub_align);

            CPairwiseAln::const_iterator prev = aln->end();
            ITERATE (CPairwiseAln, range_it, *aln) {

                // handle gaps
                if (prev != aln->end()) {
                    int q_gap = range_it->GetFirstFrom() - prev->GetFirstTo() - 1;
                    int s_gap =
                        (strand == eNa_strand_minus ?
                         prev->GetSecondFrom() - range_it->GetSecondTo() - 1 :
                         range_it->GetSecondFrom() - prev->GetSecondTo() - 1);

                    // check if this range is in the list of known introns
                    ++gap_count;
                    gap_bases += abs(q_gap - s_gap);

                    /**
                    LOG_POST(Error
                             << "  q_gap=" << q_gap
                             << "  s_gap=" << s_gap);
                             **/
                }
                prev = range_it;

                CRange<int> q_range = range_it->GetFirstRange();
                CRange<int> s_range = range_it->GetSecondRange();

                int s_start = s_range.GetFrom();
                int s_end   = s_range.GetTo();
                int q_pos   = q_range.GetFrom();

                int new_offs = q_pos % 3;
                for ( ;  offs != new_offs;  offs = (offs + 1) % 3) {
                    state = tbl.NextCodonState(state, 'N');
                }

                // first range is in nucleotide coordinates...
                /**
                LOG_POST(Error << "  range: "
                         << range_it->GetFirstFrom() << ".."
                         << range_it->GetFirstTo() << " x "
                         << range_it->GetSecondFrom() << ".."
                         << range_it->GetSecondTo() << " ("
                         << (range_it->IsReversed() ? "-" : "+") << ")"

                         << "  qspan=" << (int)(range_it->GetLength() / 3)
                         << " / " << range_it->GetLength() % 3
                        );
                        **/

                CRef<CSeq_loc> loc =
                    genomic_bsh.GetRangeSeq_loc(s_start, s_end, strand);
                CSeqVector genomic_vec(*loc, *scope, CBioseq_Handle::eCoding_Iupac);
                CSeqVector_CI vec_it(genomic_vec);

                for ( ;  s_start <= s_end;  ++s_start, ++q_pos, ++vec_it) {
                    state = tbl.NextCodonState(state, *vec_it);
                    if (offs % 3 == 2) {
                        unsigned char prot = prot_vec[(int)(q_pos / 3)];
                        unsigned char xlate = tbl.GetCodonResidue(state);

                        int this_score = full_matrix.s[prot][xlate];

                        num_match += (prot == xlate);
                        num_mismatch += (prot != xlate);
                        num_positives += (this_score > 0);
                        num_negatives += (this_score <= 0);
                        score += this_score;

                        /**
                        LOG_POST(Error
                                 << "  q_pos=" << q_pos
                                 << "  s_pos=" << s_start
                                 << "  prot_idx=" << (int)(q_pos / 3)
                                 << "  prot=" << prot
                                 << "  xlate=" << xlate
                                 << "  score=" << score
                                 << (this_score < 0 ? "  negative" : "")
                                 << (prot != xlate ? "  mismatch" : ""));
                                 **/
                    }

                    offs = (offs + 1) % 3;
                }
            }
        }

        // adjust score for gaps
        // HACK: this isn't exactly correct; it overestimates scores because we
        // don't have full accounting of composition based statistics, etc.
        // It's close enough, though
        /**
        LOG_POST(Error
                 << "  raw score = " << score
                 << "  gap openings = " << gap_count
                 << "  gap bases = " << gap_bases
                 << "  positives = " << num_positives << '(' << num_positives*3 << ')'
                 << "  negatives = " << num_negatives << '(' << num_negatives*3 << ')'
                 << "  match = " << num_match << '(' << num_match*3 << ')'
                 << "  mismatch = " << num_mismatch << '(' << num_mismatch*3 << ')'
                 << "  gap bases = " << gap_bases
                 << "  score = "
                 << score - (int)(gap_count * 11 + gap_bases * 0.33));
                 **/

        score -=
            gap_count * 11 /* gap open cost */
            + gap_bases * 0.33 /* gap extend cost */;

        return score;
    }

private:
    string m_Matrix;
};


//////////////////////////////////////////////////////////////////////////////

class CScore_BlastRatio : public CScoreLookup::IScore
{
public:
    CScore_BlastRatio(CScoreLookup &lookup)
    : m_ScoreLookup(lookup)
    {
    }

    virtual void PrintHelp(CNcbiOstream& ostr) const
    {
        ostr <<
            "Adjusted protein score (ratio of actual score to perfect score)";
    }

    virtual EComplexity GetComplexity() const { return eHard; };

    virtual double Get(const CSeq_align& align, CScope* scope) const
    {
        CSeq_id_Handle idh = CSeq_id_Handle::GetHandle(align.GetSeq_id(0));
        CBioseq_Handle bsh = scope->GetBioseqHandle(idh);
    
        ///
        /// compute the BLAST score
        ///
        int score = m_ScoreLookup.GetBlastScore(*scope, align);
    
        ///
        /// compute the BLAST score for a degenerate perfect alignment for
        /// this sequence
        ///
        CSeq_align perfect_align;
        CDense_seg& seg = perfect_align.SetSegs().SetDenseg();
        CRef<CSeq_id> id(new CSeq_id);
        id->Assign(*idh.GetSeqId());
        seg.SetIds().push_back(id);
        seg.SetIds().push_back(id);
        seg.SetNumseg(1);
        seg.SetStarts().push_back(0);
        seg.SetStarts().push_back(0);
        seg.SetLens().push_back(bsh.GetBioseqLength());
    
        double perfect_score =
            m_ScoreLookup.GetBlastScore(*scope, perfect_align);
        return perfect_score ? score / perfect_score : 0;
    }

private:
    CScoreLookup &m_ScoreLookup;
};


class CScore_EdgeExonInfo : public CScoreLookup::IScore
{
public:
    enum EEdge {e5Prime, e3Prime};

    enum EInfoType {ePercentIdentity, eLength};

    CScore_EdgeExonInfo(EEdge edge, EInfoType type)
    : m_Edge(edge), m_InfoType(type)
    {}

    virtual void PrintHelp(CNcbiOstream& ostr) const
    {
        ostr << (m_InfoType == eLength ? "Length" : "Identity percentage")
             << " of the " << (m_Edge == e5Prime ? "5'" : "3'")
             << " exon.  Note that this score has "
            "meaning only for Spliced-seg alignments, as would be generated "
            "by Splign or ProSplign, and only if it has at least one intron.";
    }

    virtual EComplexity GetComplexity() const { return eEasy; };

    virtual bool IsInteger() const { return m_InfoType == eLength; };

    virtual double Get(const CSeq_align& align, CScope* scope) const
    {
        if (!align.GetSegs().IsSpliced() ||
            align.GetSegs().GetSpliced().GetExons().size() == 1)
        {
            NCBI_THROW(CSeqalignException, eUnsupported,
              "CScore_EdgeExonInfo: "
              "valid only for spliced-seg alignments with at least one intron");
        }
        const CSpliced_seg::TExons &exons =
            align.GetSegs().GetSpliced().GetExons();
        CConstRef<CSpliced_exon> exon = m_Edge == e5Prime ? exons.front()
                                                          : exons.back();
        if (m_InfoType == eLength) {
            return exon->GetGenomic_end() - exon->GetGenomic_start() + 1;
        } else {
            if (exon->IsSetScores()) {
                ITERATE (CScore_set::Tdata, score_it, exon->GetScores().Get()) {
                    if ((*score_it)->CanGetId() && (*score_it)->GetId().IsStr()
                        && (*score_it)->GetId().GetStr() == "idty")
                    {
                        return (*score_it)->GetValue().GetReal() * 100;
                    }
                }
            }
            /// Exon percent identity not stored; calculate it
            TSeqRange product_span;
            product_span.Set(exon->GetProduct_start().AsSeqPos(),
                             exon->GetProduct_end().AsSeqPos());
            return CScoreBuilder().GetPercentIdentity(*scope, align,
                                                      product_span);
        }
    }

private:
    EEdge m_Edge;
    EInfoType m_InfoType;
};

class CScore_GeneID : public CScoreLookup::IScore
{
public:
    CScore_GeneID(int row)
    : m_Row(row)
    {
    }

    virtual void PrintHelp(CNcbiOstream& ostr) const
    {
        ostr << "Gene ID of " << (m_Row == 0 ? "query" : "subject");
    }

    virtual EComplexity GetComplexity() const { return eEasy; };

    virtual bool IsInteger() const { return true; };

    virtual double Get(const CSeq_align& align, CScope* scope) const
    {
        CBioseq_Handle bsh = scope->GetBioseqHandle(align.GetSeq_id(m_Row));
        CFeat_CI gene_it(bsh, CSeqFeatData::e_Gene);
        if (!gene_it) {
            NCBI_THROW(CException, eUnknown, "No gene feature");
        }

        CMappedFeat gene = *gene_it;
        if (++gene_it) {
            NCBI_THROW(CException, eUnknown, "Multiple gene features");
        }

        return gene.GetNamedDbxref("GeneID")->GetTag().GetId();
    }

private:
    int m_Row;
};

void CScoreLookup::x_Init()
{
    m_Scores.insert
        (TScoreDictionary::value_type
         ("align_length_ungap",
          CIRef<IScore>(new CScore_AlignLength(false /* include gaps */))));
    m_Scores.insert
        (TScoreDictionary::value_type
         ("gap_count",
          CIRef<IScore>(new CScore_GapCount(false))));
    m_Scores.insert
        (TScoreDictionary::value_type
         ("gap_basecount",
          CIRef<IScore>(new CScore_GapCount(true))));
    m_Scores.insert
        (TScoreDictionary::value_type
         ("query_gap_length",
          CIRef<IScore>(new CScore_GapCount(true, 0))));
    m_Scores.insert
        (TScoreDictionary::value_type
         ("subject_gap_length",
          CIRef<IScore>(new CScore_GapCount(true, 1))));
    m_Scores.insert
        (TScoreDictionary::value_type
         ("product_gap_length",
          CIRef<IScore>(new CScore_GapCount(true, 0, true))));
    m_Scores.insert
        (TScoreDictionary::value_type
         ("genomic_gap_length",
          CIRef<IScore>(new CScore_GapCount(true, 1, true))));
    m_Scores.insert
        (TScoreDictionary::value_type
         ("frame",
          CIRef<IScore>(new CScore_FrameShifts())));
    m_Scores.insert
        (TScoreDictionary::value_type
         ("qframe",
          CIRef<IScore>(new CScore_FrameShifts(0))));
    m_Scores.insert
        (TScoreDictionary::value_type
         ("sframe",
          CIRef<IScore>(new CScore_FrameShifts(1))));
    m_Scores.insert
        (TScoreDictionary::value_type
         ("symmetric_overlap",
          CIRef<IScore>(new CScore_SymmetricOverlap(
                            CScore_SymmetricOverlap::e_Avg))));
    m_Scores.insert
        (TScoreDictionary::value_type
         ("symmetric_overlap_min",
          CIRef<IScore>(new CScore_SymmetricOverlap(
                            CScore_SymmetricOverlap::e_Min))));
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

    m_Scores.insert
        (TScoreDictionary::value_type
         ("prosplign_tblastn_score",
          CIRef<IScore>(new CScore_TblastnScore("BLOSUM62" /* HACK: no user choice */))));

    m_Scores.insert
        (TScoreDictionary::value_type
         ("blast_score_ratio",
          CIRef<IScore>(new CScore_BlastRatio(*this))));

    m_Scores.insert
        (TScoreDictionary::value_type
         ("start_codon",
          CIRef<IScore>(new CScore_StartStopCodon(true))));

    m_Scores.insert
        (TScoreDictionary::value_type
         ("stop_codon",
          CIRef<IScore>(new CScore_StartStopCodon(false))));

    m_Scores.insert
        (TScoreDictionary::value_type
         ("5prime_exon_len",
          CIRef<IScore>(new CScore_EdgeExonInfo(
                            CScore_EdgeExonInfo::e5Prime,
                            CScore_EdgeExonInfo::eLength))));

    m_Scores.insert
        (TScoreDictionary::value_type
         ("3prime_exon_len",
          CIRef<IScore>(new CScore_EdgeExonInfo(
                            CScore_EdgeExonInfo::e3Prime,
                            CScore_EdgeExonInfo::eLength))));

    m_Scores.insert
        (TScoreDictionary::value_type
         ("5prime_exon_pct_identity",
          CIRef<IScore>(new CScore_EdgeExonInfo(
                            CScore_EdgeExonInfo::e5Prime,
                            CScore_EdgeExonInfo::ePercentIdentity))));

    m_Scores.insert
        (TScoreDictionary::value_type
         ("3prime_exon_pct_identity",
          CIRef<IScore>(new CScore_EdgeExonInfo(
                            CScore_EdgeExonInfo::e3Prime,
                            CScore_EdgeExonInfo::ePercentIdentity))));

    m_Scores.insert
        (TScoreDictionary::value_type
         ("query_geneid",
          CIRef<IScore>(new CScore_GeneID(0))));
    m_Scores.insert
        (TScoreDictionary::value_type
         ("subject_geneid",
          CIRef<IScore>(new CScore_GeneID(1))));
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
    CSeq_align::TScoreNameMap::const_iterator score_it =
        CSeq_align::ScoreNameMap().find(score_name);
    if (score_it != CSeq_align::ScoreNameMap().end()) {
        return CSeq_align::HelpText(score_it->second);
    }

    TScoreDictionary::const_iterator token_it = m_Scores.find(score_name);
    if (token_it != m_Scores.end()) {
        m_ScoresUsed.insert(score_name);
        CNcbiOstrstream os;
        token_it->second->PrintHelp(os);
        return string(CNcbiOstrstreamToString(os));
    }
    
    return "assumed to be a score on the Seq-align";
}

CScoreLookup::IScore::EComplexity CScoreLookup::
Complexity(const string &score_name)
{
    CSeq_align::TScoreNameMap::const_iterator score_it =
        CSeq_align::ScoreNameMap().find(score_name);
    if (score_it != CSeq_align::ScoreNameMap().end()) {
        return IScore::eEasy;
    }

    TScoreDictionary::const_iterator token_it = m_Scores.find(score_name);
    if (token_it != m_Scores.end()) {
        return token_it->second->GetComplexity();
    }
    
    NCBI_THROW(CAlgoAlignUtilException, eScoreNotFound, score_name);
}

bool CScoreLookup::IsIntegerScore(const objects::CSeq_align& align,
                                  const string &score_name)
{
    CSeq_align::TScoreNameMap::const_iterator score_it =
        CSeq_align::ScoreNameMap().find(score_name);
    if (score_it != CSeq_align::ScoreNameMap().end()) {
        return CSeq_align::IsIntegerScore(score_it->second);
    }

    TScoreDictionary::const_iterator token_it = m_Scores.find(score_name);
    if (token_it != m_Scores.end()) {
        return token_it->second->IsInteger();
    }
    
    ITERATE (CSeq_align::TScore, stored_score_it, align.GetScore()) {
        if ((*stored_score_it)->CanGetValue() &&
            (*stored_score_it)->CanGetId() &&
            (*stored_score_it)->GetId().IsStr() &&
            (*stored_score_it)->GetId().GetStr() == score_name)
        {
            return (*stored_score_it)->GetValue().IsInt();
        }
    }
    return false;
}

double CScoreLookup::GetScore(const objects::CSeq_align& align,
                              const string &score_name)
{
    double score;
    if (align.GetNamedScore(score_name, score)) {
        return score;
    }

    if (m_Scope.IsNull()) {
        m_Scope.Reset(new CScope(*CObjectManager::GetInstance()));
        m_Scope->AddDefaults();
    }

    /// Score not found in alignmnet; look for it among built-in scores
    CSeq_align::TScoreNameMap::const_iterator score_it =
        CSeq_align::ScoreNameMap().find(score_name);
    if (score_it != CSeq_align::ScoreNameMap().end()) {
        return ComputeScore(*m_Scope, align, score_it->second);
    }

    /// Not a built-in score; look for it among computed tokens
    TScoreDictionary::const_iterator token_it = m_Scores.find(score_name);
    if (token_it != m_Scores.end()) {
        m_ScoresUsed.insert(score_name);
        return token_it->second->Get(align, &*m_Scope);
    }

    NCBI_THROW(CAlgoAlignUtilException, eScoreNotFound, score_name);
}

END_NCBI_SCOPE

