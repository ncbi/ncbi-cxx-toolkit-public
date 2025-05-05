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
 * Authors:  Alex Astashyn
 *
 * File Description:
 *
 */

#include <ncbi_pch.hpp>
#include <algo/sequence/gene_model.hpp>
#include <corelib/ncbitime.hpp>
#include <objmgr/object_manager.hpp>
#include <objmgr/scope.hpp>
#include <objmgr/bioseq_handle.hpp>
#include <objmgr/annot_ci.hpp>
#include <objmgr/feat_ci.hpp>
#include <objmgr/seq_loc_mapper.hpp>
#include <objmgr/util/sequence.hpp>
#include <objmgr/util/feature.hpp>
#include <objmgr/seq_vector.hpp>

#include <objects/general/Dbtag.hpp>
#include <objects/general/User_object.hpp>
#include <objects/seqalign/Dense_seg.hpp>
#include <objects/seqalign/Product_pos.hpp>
#include <objects/seqalign/Prot_pos.hpp>
#include <objects/seqalign/Seq_align.hpp>
#include <objects/seqalign/Spliced_exon_chunk.hpp>
#include <objects/seqalign/Spliced_exon.hpp>
#include <objects/seqalign/Spliced_seg.hpp>
#include <objects/seqalign/Spliced_seg_modifier.hpp>
#include <objects/seqalign/Splice_site.hpp>
#include <objects/seqfeat/seqfeat__.hpp>
#include <objects/seqfeat/SeqFeatXref.hpp>
#include <objects/seqfeat/Org_ref.hpp>
#include <objects/seqfeat/Prot_ref.hpp>
#include <objects/seqloc/Seq_id.hpp>
#include <objects/seqloc/Seq_interval.hpp>
#include <objects/seqloc/Packed_seqint.hpp>
#include <objects/seqloc/Seq_loc_mix.hpp>
#include <objects/seqloc/Seq_loc_equiv.hpp>
#include <objects/seq/seq__.hpp>
#include <objects/general/Object_id.hpp>
#include <util/sequtil/sequtil_convert.hpp>

BEGIN_NCBI_SCOPE
USING_SCOPE(objects);


/// Recursively convert empty container-locs to null-locs, 
/// drop null sublocs from containers, and unwrap singleton containers
void Canonicalize(CSeq_loc& loc)
{
    if(loc.IsMix()) {
        NON_CONST_ITERATE(CSeq_loc::TMix::Tdata, it, loc.SetMix().Set()) {
            Canonicalize(**it);
        }
        //erase NULL sublocs
        CSeq_loc::TMix::Tdata::iterator dest = loc.SetMix().Set().begin();
        NON_CONST_ITERATE(CSeq_loc::TMix::Tdata, it, loc.SetMix().Set()) {
            CRef<CSeq_loc> subloc = *it;
            if(!subloc->IsNull()) {
                *dest = subloc;
                dest++;
            }
        }
        loc.SetMix().Set().erase(dest, loc.SetMix().Set().end());
        
        if(loc.GetMix().Get().size() == 1) {
            CRef<CSeq_loc> content_loc = loc.SetMix().Set().front();
            loc.Assign(*content_loc);
        } else if(loc.GetMix().Get().size() == 0) {
            loc.SetNull();
        }
    } else if(loc.IsPacked_int()) {
        if(loc.GetPacked_int().Get().size() == 1) {
            CRef<CSeq_interval> seq_int = loc.GetPacked_int().Get().front();
            loc.SetInt(*seq_int);
        } else if(loc.GetPacked_int().Get().size() == 0) {
            loc.SetNull();
        }
    }
}


/// retrun true iff abutting on query (in nucpos-coords)
bool AreAbuttingOnProduct(const CSpliced_exon& exon1,
                          const CSpliced_exon& exon2)
{
    TSeqPos max_start = max(exon1.GetProduct_start().AsSeqPos(),
                            exon2.GetProduct_start().AsSeqPos());

    TSeqPos min_stop  = min(exon1.GetProduct_end().AsSeqPos(),
                            exon2.GetProduct_end().AsSeqPos());

    return max_start == min_stop + 1;
}

/// ::first and ::second indicate partialness 
/// for of a loc or an exon, 5' and 3' end respectively.
typedef pair<bool, bool> T53Partialness; 

/// Return whether 5' and/or 3' end of exon is partial based on
/// consensus splicing with upstream/downstream exons.
T53Partialness GetExonPartialness(const CSeq_align& spliced_aln,
                                  const CSpliced_exon& target_exon)
{
    bool is_5p_partial(false);
    bool is_3p_partial(false);

    CConstRef<CSpliced_exon> prev_exon;
    ITERATE(CSpliced_seg::TExons, it, spliced_aln.GetSegs().GetSpliced().GetExons()) {
        CConstRef<CSpliced_exon> current_exon = *it;
        if(  !prev_exon
           || AreAbuttingOnProduct(*prev_exon, *current_exon)) 
        {
            prev_exon = current_exon;
            continue;
        }
        
        //gap between exons. Determine which exon is partial based on consensus splice
        const bool is_consensus_donor = 
            prev_exon->IsSetDonor_after_exon() 
         &&   prev_exon->GetDonor_after_exon().GetBases() == "GT";

        const bool is_consensus_acceptor = 
            current_exon->IsSetAcceptor_before_exon()
         &&   current_exon->GetAcceptor_before_exon().GetBases() == "AG";

        if(current_exon == CConstRef<CSpliced_exon>(&target_exon)
           && (!is_consensus_acceptor || is_consensus_donor))
        {
            is_5p_partial = true;
        }

        if(prev_exon == CConstRef<CSpliced_exon>(&target_exon)
           && (!is_consensus_donor || is_consensus_acceptor))
        {
            is_3p_partial = true;
        }

        prev_exon = current_exon;
    }

    return T53Partialness(is_5p_partial, is_3p_partial);
}

size_t GetUnalignedLength_3p(const CSeq_align& spliced_aln)
{
    return spliced_aln.GetSegs().GetSpliced().IsSetPoly_a()         
                ? 0

         : spliced_aln.GetSeqStrand(0) == eNa_strand_minus          
                ? spliced_aln.GetSeqStart(0)

         : spliced_aln.GetSegs().GetSpliced().IsSetProduct_length() 
                ? spliced_aln.GetSegs().GetSpliced().GetProduct_length() 
                  - spliced_aln.GetSeqStop(0) - 1

         :        0;
}

size_t GetUnalignedLength_5p(const CSeq_align& spliced_aln)
{
    return spliced_aln.GetSeqStrand(0) != eNa_strand_minus          
                ? spliced_aln.GetSeqStart(0)

         : spliced_aln.GetSegs().GetSpliced().IsSetProduct_length() 
                ? spliced_aln.GetSegs().GetSpliced().GetProduct_length() 
                  - spliced_aln.GetSeqStop(0) - 1

         :        0;
}

/// Return whether 5' and/or 3' end of exons-loc is partial
/// based on unaligned tails in case of RNA,
///       or overlap of product-cds-loc with unaligned tails, in case of CDS.
T53Partialness GetTerminalPartialness(
        const CSeq_align& spliced_aln,
        CConstRef<CSeq_loc> product_cds_loc,
        size_t unaligned_ends_partialness_thr)
{
    T53Partialness partialness(false, false);
    if(!product_cds_loc) {
        // For RNA set partialness based on whether the unaligned 
        // ends are longer than allow_terminal_unaligned_bases
        partialness.first = GetUnalignedLength_5p(spliced_aln) 
                           > unaligned_ends_partialness_thr;

        partialness.second = GetUnalignedLength_3p(spliced_aln) 
                           > unaligned_ends_partialness_thr;
    } else {
        // cds-exons 5p/3p-terminal partialness is based on whether the 
        // product-cds-loc terminals are covered by the alignment.
        TSeqPos cds_start = product_cds_loc->GetStart(eExtreme_Positional);
        TSeqPos cds_stop  = product_cds_loc->GetStop(eExtreme_Positional);
        if (spliced_aln.GetSegs().GetSpliced().GetProduct_type()
            == CSpliced_seg::eProduct_type_protein) {
            cds_start *= 3;
            cds_stop = cds_stop*3+2;
        }
        
        bool start_covered = false;
        bool stop_covered = false;
        ITERATE(CSpliced_seg::TExons, it, 
                spliced_aln.GetSegs().GetSpliced().GetExons()) 
        {
            const CSpliced_exon& exon = **it;
            start_covered |= cds_start >= exon.GetProduct_start().AsSeqPos()
                          && cds_start <= exon.GetProduct_end  ().AsSeqPos();

            stop_covered  |= cds_stop  >= exon.GetProduct_start().AsSeqPos()
                          && cds_stop  <= exon.GetProduct_end  ().AsSeqPos();
        }

        partialness.first  = !start_covered 
                          || product_cds_loc->IsPartialStart(eExtreme_Positional);

        partialness.second = !stop_covered
                          || product_cds_loc->IsPartialStop(eExtreme_Positional);

        if(spliced_aln.GetSeqStrand(0) == eNa_strand_minus) {
            swap(partialness.first, partialness.second);
        }
    }
    return partialness;
}

void AugmentPartialness(CSeq_loc& loc, T53Partialness partialness)
{
    if(partialness.first) {
        loc.SetPartialStart(true, eExtreme_Biological);
    }
    if(partialness.second) {
        loc.SetPartialStop(true, eExtreme_Biological);
    }
}


#if 1

/* 
 * GP-13080
 *
 * Tweak the biostops of projected exon's subintervals 
 * while preserving the reading frame.
 *
 * Stitch all non-frameshifting indels.
 *
 * Truncate all overlaps and close all gops down to 1 or 2 bp gap
 * as to preserve the frame.
 *
 * Note:
 * This approach suffers from some problems - we could be truncating
 * bases in perfectly good codons instead of truncating 
 * bases strictly from affected codons that contain product-ins.
 * An alternative approach is possible, but will require rewriting 
 * the whole projection code from scratch:
 *
 * Alternative approach:
 *   Implement a function  (spliced-seg, product-cds-loc) -> tweaked-product-cds
 *   where the output CDS has only non-frameshifting gaps and has lossless
 *   mapping through the spliced-seg.
 *   (i.e. project gaps onto query and tweak them to codon boundaries; 
 *   pay special attention to ribosomal-slippage case).
 *
 *   When projecting CDS, use tweaked-product-cds to map to genome, and 
 *   collapse mod-3 exonic gaps.
 *
 *   When projecting mRNA, replace the cds-range on mRNA with 
 *   tweaked-product-cds, and map like CDS. Afterwards, collapse all
 *   indels UTRs.
 *
 *   When projecting RNA, map the product-range and collase all indels.
 *
 */
struct NTweakExon
{
public:
    static CRef<CSeq_loc> TweakExon(const CSeq_loc& orig_loc, bool convert_overlaps)
    {
        if(!orig_loc.IsPacked_int()) {
            NCBI_THROW(CException, eUnknown, "Expected packed-int");
        }

        CRef<CSeq_loc> loc = Clone(orig_loc);

        // Note: Adjusting-biostops is done twice: first time 
        // before subsuming micro-intervals (as they're not
        // 'micro' prior to this), and after subsuming, which
        // may have elongated some intervals, while the 
        // next step expects upper bound of 2bp on all overlaps.

        if(false /*orig_loc.GetStart(eExtreme_Positional) == 1988953*/) {
            NcbiCerr << "Orig:  " << AsString(loc->GetPacked_int()) << "\n";
            AdjustBiostops(loc->SetPacked_int());
            NcbiCerr << "Adj1:  " << AsString(loc->GetPacked_int()) << "\n";
            SubsumeMicroIntervals(loc->SetPacked_int());
            NcbiCerr << "Subs:  " << AsString(loc->GetPacked_int()) << "\n";
            AdjustBiostops(loc->SetPacked_int());
            NcbiCerr << "Adj2:  " << AsString(loc->GetPacked_int()) << "\n";
            if (convert_overlaps) {
                ConvertOverlapsToGaps(loc->SetPacked_int());
                NcbiCerr << "Ovlp:  " << AsString(loc->GetPacked_int()) << "\n";
            }
            CollapseNonframeshiftting(loc->SetPacked_int());

            NcbiCerr << "Final: " << AsString(loc->GetPacked_int()) << "\n";
            NcbiCerr << "Tweaked: " 
                     << (orig_loc.Equals(*loc) ? "equal" : "not-equal")
                     << "\n\n";
        } else {
            AdjustBiostops(loc->SetPacked_int());
            SubsumeMicroIntervals(loc->SetPacked_int());
            AdjustBiostops(loc->SetPacked_int());
            if (convert_overlaps) {
                ConvertOverlapsToGaps(loc->SetPacked_int());
            }
            CollapseNonframeshiftting(loc->SetPacked_int());
        }

        Validate(orig_loc, *loc);
        return loc;
    }

    template<typename T>
    static CRef<T> Clone(const T& x)
    {
        return CRef<T>(SerialClone<T>(x));
    }


    // advance iterator by d (can be negative);
    // if out of bounds, assign end.
    template<typename iterator_t>
    static void safe_advance(
        iterator_t begin,
        iterator_t end,
        iterator_t& it,
        Int8 d)
    {
        if(   (d < 0 && distance(begin, it) < abs(d))
           || (d > 0 && distance(it,   end) < abs(d))) 
        {
            it = end;
        } else {
            std::advance(it, d);
        }
    }


    // return element at iterator advanced by d.
    // if out of bounds, return sentinel value
    template<typename container_t>
    static typename container_t::value_type rel_at(
            const container_t& container,
            typename container_t::const_iterator it,
            Int8 delta,
            typename container_t::value_type default_value)
    {
        safe_advance(container.begin(), 
                     container.end(), 
                     it, delta);

        return it == container.end() ? default_value : *it;
    }


    static CRef<CSeq_interval> Next(
            const CPacked_seqint::Tdata& seqints,
            const CPacked_seqint::Tdata::const_iterator it)
    {
        return rel_at(seqints, it, 1, CRef<CSeq_interval>(NULL));
    }


    static CRef<CSeq_interval> Prev(
            const CPacked_seqint::Tdata& seqints,
            const CPacked_seqint::Tdata::const_iterator it)
    {
        return rel_at(seqints, it, -1, CRef<CSeq_interval>(NULL));
    }


    // Will subsume micro-intervals smaller than this into upstream
    // neighboring intervals; This number can't be smaller than 4,
    // because an interval must be able to survive truncation by
    // 3 bases when changing overlaps to gaps
    static const TSeqPos k_min_len = 4;
    

    // Will not subsume intervals of this length or longer.
    // Otherwise can subsume if this creates a nonframeshifting gap
    static const TSeqPos k_keep_len = 6;


    static TSeqPos GetBiostartsDelta(
            const CSeq_interval& upst,
            const CSeq_interval& downst)
    {
        TSignedSeqPos d = downst.GetStart(eExtreme_Biological)
                        - upst.GetStart(eExtreme_Biological);

        return d * (MinusStrand(upst) ? -1 : 1);
    }


    static CRef<CSeq_interval> Collapse(
            const CSeq_interval& a,
            const CSeq_interval& b)
    {
        CRef<CSeq_interval> c = Clone(a);
        c->SetFrom( min(a.GetFrom(), b.GetFrom()));
        c->SetTo(   max(a.GetTo(),   b.GetTo()));
        return c;
    }


    static bool MinusStrand(const CSeq_interval& seqint)
    {
        return seqint.IsSetStrand() 
            && seqint.GetStrand() == eNa_strand_minus;
    }


    static bool SameIdAndStrand(
            const CSeq_interval& a,
            const CSeq_interval& b)
    {
        return a.GetId().Equals(b.GetId())
            && MinusStrand(a) == MinusStrand(b);
    }


    // return true iff subsuming curr into prev will result
    // in a non-frameshifting indel between prev and next.
    static bool CanCreateNonframeshiftingGap(
            const CSeq_interval& prev,
            const CSeq_interval& curr,
            const CSeq_interval& next)
    {
        TSignedSeqPos g = GetBiostartsDelta(prev, next) 
                        - prev.GetLength()
                        - curr.GetLength();
        return (g % 3) == 0;
    }

    // can be negative in case of overlap
    static TSignedSeqPos GetGapLength(
            const CSeq_interval& prev,
            const CSeq_interval& curr)
    {
        return GetBiostartsDelta(prev, curr) 
             - prev.GetLength();
    }


    static void SetBioStop(CSeq_interval& seqint, TSeqPos pos)
    {
        (MinusStrand(seqint) ? seqint.SetFrom(pos)
                             : seqint.SetTo(pos));
    }


    static void SetBioStart(CSeq_interval& seqint, TSeqPos pos)
    {
        (MinusStrand(seqint) ? seqint.SetTo(pos)
                             : seqint.SetFrom(pos));
    }


    // amount < 0 -> truncate 3'end upstream; else extend downstream
    static void AdjustBioStop(
            CSeq_interval& seqint, 
            TSignedSeqPos amt)
    {
        amt *= MinusStrand(seqint) ? -1 : 1;
        amt += seqint.GetStop(eExtreme_Biological);
        SetBioStop(seqint, amt);
    }


    // amount < 0 -> extend 5'end upstream; else truncate downstream
    static void AdjustBioStart(
            CSeq_interval& seqint, 
            TSignedSeqPos amt)
    {
        amt *= MinusStrand(seqint) ? -1 : 1;
        amt += seqint.GetStart(eExtreme_Biological);
        SetBioStart(seqint, amt);
    }


    static void CheckIdAndStrand(const CPacked_seqint& ps)
    {
        ITERATE(CPacked_seqint::Tdata, it, ps.Get()) {
            CRef<CSeq_interval> prev = Prev(ps.Get(), it);
            if(prev && !SameIdAndStrand(*prev, **it)) {
                NcbiCerr << MSerial_AsnText << ps;
                NCBI_THROW(CException, 
                           eUnknown, 
                           "Expected same seq-id and strand");
            }
        }
    }


    // maximally close gaps and truncate overlaps in multiples of three.
    static void AdjustBiostops(CPacked_seqint& ps)
    {
        NON_CONST_ITERATE(CPacked_seqint::Tdata, it, ps.Set()) {
            CRef<CSeq_interval> prev = Prev(ps.Set(), it);

            if(!prev) {
                continue;
            }

            const TSignedSeqPos indel = GetGapLength(*prev, **it);
            AdjustBioStop(*prev, indel / 3 * 3);
        }
    }


    // Subsume micro-intervals into upstream predecessors, as long as
    // it does not affect the terminals.
    static void SubsumeMicroIntervals(CPacked_seqint& ps)
    {
        CPacked_seqint::Tdata& seqints = ps.Set();
        CPacked_seqint::Tdata::iterator dest = seqints.begin();

        NON_CONST_ITERATE(CPacked_seqint::Tdata, it, ps.Set()) {
            CRef<CSeq_interval> current = *it;
            CRef<CSeq_interval> next = Next(seqints, it);

            if(it == dest) {
                continue; // no predecessor
            }

            const bool can_subsume =
                !next                               ? false
               : current->GetLength() <  k_min_len  ? true
               : current->GetLength() >= k_keep_len ? false
               : CanCreateNonframeshiftingGap(
                       **dest, *current, *next)     ? true
               :                                      false;                     

            // Note: if we don't have next, that means that this 
            // interval is last. We are not allowed to subsume it
            // because dropping it and adjusting the upstream
            // interval's length to compensate will affect the
            // packed_seqint's biostop. GP-15942

            if(can_subsume) {
                // drop current and extend prev to compensate
                AdjustBioStop(**dest, current->GetLength());
            } else {
                // keep current
                ++dest;
                *dest = current;
            }
        }

        seqints.erase(seqints.end() == dest ? dest : ++dest,
                      seqints.end());
    }


    // Precondition: overlaps are at most 2bp
    static void ConvertOverlapsToGaps(CPacked_seqint& ps)
    {
        NON_CONST_ITERATE(CPacked_seqint::Tdata, it, ps.Set()) {
            CRef<CSeq_interval> current = *it;
            CRef<CSeq_interval> prev = Prev(ps.Set(), it);

            if(!prev) {
                continue;
            }

            const TSignedSeqPos overlap = -1 * GetGapLength(*prev, *current);

            if(overlap <= 0) {
                continue; //gap
            } else if(overlap > 2) {
                NcbiCerr << MSerial_AsnText << ps;
                NCBI_THROW(CException, eUnknown, "Unexpected overlap");
            } else if(prev->GetLength() > 3) {
                AdjustBioStop(*prev, -3);
            } else if(current->GetLength() > 3) {
                AdjustBioStart(*current, 3);
            }
        }
    }


    static void CollapseNonframeshiftting(CPacked_seqint& ps)
    {
        CPacked_seqint::Tdata& seqints = ps.Set();
        CPacked_seqint::Tdata::iterator dest = seqints.begin();

        NON_CONST_ITERATE(CPacked_seqint::Tdata, it, ps.Set()) {
            if(it == dest) {
                continue;
            }

            if(GetGapLength(**dest, **it) % 3 == 0) {
                *dest = Collapse(**dest, **it);
            } else {
                ++dest;
                *dest = *it;
            }
        }

        seqints.erase(seqints.end() == dest ? dest : ++dest,
                      seqints.end());
    }

public:
    // It's difficult to make sense of a printed seq-loc ASN 
    // or label the way it is done in the toolkit, especially 
    // when the coordinates are large.
    //
    // This prints intervals as comma-delimited sequence of tuples 
    // "header,length" in bio-order, where header is either gap 
    // length to previous interval iff seq-id and strands are the same, 
    // and otherwise the header is seq-id@signed-biostart.
    //
    // E.g. NC_000001.11@-12304058:100,+1,150,-2,300
    //                   ^strand
    //                    ^biostart
    //                             ^      ^      ^ lengths (unsigned)
    //                                  ^gap   ^overlap    (signed)
    static string AsString(const CPacked_seqint& packed_seqint)
    {
        if(packed_seqint.Get().empty()) {
            return "Empty-Packed-seqint";
        }

        CNcbiOstrstream ostr;
        CConstRef<CSeq_interval> prev_seqint;
        ITERATE(CPacked_seqint::Tdata, it, packed_seqint.Get()) {
            CConstRef<CSeq_interval> seqint = *it;

            if(prev_seqint && SameIdAndStrand(*prev_seqint, *seqint)) {
                TSignedSeqPos d = 
                    (seqint->GetStart(eExtreme_Biological))
                  - (  prev_seqint->GetStop(eExtreme_Biological) 
                     + (MinusStrand(*seqint) ? -1 : 1));

                d *= MinusStrand(*seqint) ? -1 : 1;

                ostr << "," 
                     << (d < 0 ? "-" : "+") << abs(d)
                     << "," << seqint->GetLength();
            } else {
                ostr << (!prev_seqint ? "" : ",")
                     << seqint->GetId().GetSeqIdString() 
                     << "@" << (MinusStrand(*seqint) ? "-" : "+")
                     << seqint->GetStart(eExtreme_Biological) + 1
                     << ":" << seqint->GetLength();
            }

            prev_seqint = seqint;
        }

        return CNcbiOstrstreamToString(ostr);
    }


    static void Validate(const CSeq_loc& orig_loc, const CSeq_loc& final_loc)
    {
        if(   sequence::GetStart(orig_loc,  NULL, eExtreme_Positional)
           != sequence::GetStart(final_loc, NULL, eExtreme_Positional)) 
        {
            NCBI_USER_THROW("Change in positional-starts");
        }


        if(   sequence::GetStop(orig_loc,  NULL, eExtreme_Positional)
           != sequence::GetStop(final_loc, NULL, eExtreme_Positional)) 
        {
            NCBI_USER_THROW("Change in positional-stops");
        }


        if(   sequence::GetLength(final_loc, NULL) % 3 
           != sequence::GetLength(orig_loc,  NULL) % 3)
        {
            NCBI_THROW(CException, eUnknown, 
                       "Logic error - frame not preserved");
        }

        string problem_str;
        CConstRef<CSeq_interval> prev_seqint(NULL);
        ITERATE(CPacked_seqint::Tdata, it, final_loc.GetPacked_int().Get()) {

            const CSeq_interval& seqint = **it;

            if(seqint.GetFrom() > seqint.GetTo()) {
                problem_str += "invalid seqint";
            }

            const TSignedSeqPos d = 
                !prev_seqint ? 1 : GetGapLength(*prev_seqint, seqint);

            if(d != 1 && d != 2) {
                //problem_str += "Gap length is not 1 or 2";

                // this can hapen if we couldn't subsume a terminal micro-interval
                // and it was too short to be truncatable, so it was left as-is.
            }

            if(!problem_str.empty()) {
                NcbiCerr << "orig_loc: "           
                         << AsString(orig_loc.GetPacked_int())
                         << "\nfinal_loc: "        
                         << AsString(final_loc.GetPacked_int())
                         << "\nproblem-int: "   
                         << MSerial_AsnText << seqint
                         << "\nproblem: " << problem_str << "\n";

                if(prev_seqint) {
                    NcbiCerr << "upstream-int: " 
                             << MSerial_AsnText << *prev_seqint;
                }
                NCBI_THROW(CException, eUnknown, problem_str);
            }

            prev_seqint = *it;
        }
    }
};




// Project exon to genomic coordinates, preserving discontinuities.
static CRef<CSeq_loc> ProjectExon_oldlogic(
        const CSpliced_exon& spliced_exon, 
        const CSeq_id& aln_genomic_id,  //of the parent alignment (if not specified in spliced_exon)
        ENa_strand aln_genomic_strand,  //of the parent alignment (if not specified in spliced_exon)
        bool convert_overlaps)
{
    CRef<CSeq_loc> exon_loc(new CSeq_loc(CSeq_loc::e_Packed_int));

    const CSeq_id& genomic_id = 
            spliced_exon.IsSetGenomic_id() ? spliced_exon.GetGenomic_id() 
                                           : aln_genomic_id;
    const ENa_strand genomic_strand = 
            spliced_exon.IsSetGenomic_strand() ? spliced_exon.GetGenomic_strand() 
                                               : aln_genomic_strand;

    //Don't have exon details - create based on exon boundaries and return.
    if(!spliced_exon.IsSetParts()) {
        exon_loc->SetInt().SetId().Assign( genomic_id);
        exon_loc->SetInt().SetStrand(      genomic_strand);
        exon_loc->SetInt().SetFrom(        spliced_exon.GetGenomic_start());
        exon_loc->SetInt().SetTo(          spliced_exon.GetGenomic_end());
        return exon_loc;
    }

    typedef vector<pair<int, int> > TExonStructure; 
        // Each element is an exon chunk comprised of alignment diag 
        // (match or mismatch run) and abutting downstream gaps.
        // ::first  is diag+query_gap  , 
        //      corresponding to the transcribed chunk length. 
        //
        // ::second is diag+subject_gap, 
        //      corresponding to distance to the start of the next chunk.

    TExonStructure exon_structure;
    bool last_is_diag = false;
    ITERATE(CSpliced_exon::TParts, it, spliced_exon.GetParts()) {
        const CSpliced_exon_chunk& chunk = **it;
        int len = chunk.IsMatch()       ? chunk.GetMatch()
                : chunk.IsMismatch()    ? chunk.GetMismatch()
                : chunk.IsDiag()        ? chunk.GetDiag()
                : chunk.IsGenomic_ins() ? chunk.GetGenomic_ins()
                : chunk.IsProduct_ins() ? chunk.GetProduct_ins()
                : 0;
        bool is_diag = chunk.IsMatch() || chunk.IsMismatch() || chunk.IsDiag();

        if(is_diag && last_is_diag) { //alternating match/mismatch runs go into the same chunk
            exon_structure.back().first += len;
            exon_structure.back().second += len;
        } else if(is_diag) {
            exon_structure.push_back(TExonStructure::value_type(len, len));
        } else {
            if(exon_structure.empty()) {
                exon_structure.push_back(TExonStructure::value_type(0, 0));
            }
            (chunk.IsProduct_ins() ? exon_structure.back().first 
                                   : exon_structure.back().second) += len;
        }
        last_is_diag = is_diag;
    }

    // make the subject values cumulative 
    // (i.e. relative to the exon boundary, 
    // rather than neighboring chunk)
    // After this, the biological start of 
    // a chunk relative to the exon-start 
    // is ::second of the previous element
    NON_CONST_ITERATE(TExonStructure, it, exon_structure) {
        if(it != exon_structure.begin()) {
            it->second += (it-1)->second;
        }   
    }

    int genomic_sign = genomic_strand == eNa_strand_minus ? -1 : 1;
    TSeqPos exon_bio_start_pos = 
        genomic_sign > 0 ? spliced_exon.GetGenomic_start() 
                         : spliced_exon.GetGenomic_end(); 
    exon_loc->SetPacked_int();
    ITERATE(TExonStructure, it, exon_structure) {
        int chunk_length = it->first;
        int chunk_offset = it == exon_structure.begin() ? 0 : (it-1)->second; 

        if(chunk_length == 0) {
            // can happen if we have a gap-only chunk 
            // (e.g. arising from truncating alignment to CDS)
            continue;
        }

        TSeqPos bio_start = exon_bio_start_pos + (chunk_offset * genomic_sign);
        TSeqPos bio_stop = [&]() -> TSeqPos
        {
            auto pos = (int64_t)bio_start + (chunk_length - 1) * genomic_sign; 
            // -1 because stop is inclusive

            // CXX-14032 
            // chunk_length (on product) may be greater than 
            // it is on subject, so when it abuts the sequence origin
            // on negative strand, it may overshoot the origin and 
            // become negative in signed space or close to 2^32 as TSeqPos.
            //
            // We will adjust the chunk in frame-preserving manner.
            if (pos < 0) {
                pos = ((pos % 3) + 3) % 3; // same as: while (pos < 0) { pos += 3; }
            }
            return pos;
        }();
   
        CRef<CSeq_interval> chunk(new CSeq_interval);
        chunk->SetId().Assign(genomic_id);
        chunk->SetStrand(genomic_strand);
        chunk->SetFrom(genomic_sign > 0 ? bio_start : bio_stop);
        chunk->SetTo(genomic_sign > 0 ? bio_stop : bio_start);
        exon_loc->SetPacked_int().Set().push_back(chunk);
    }

    exon_loc = NTweakExon::TweakExon(*exon_loc, convert_overlaps);
    return exon_loc;
}

#endif


// Each block represents a diag followed by downstream gap(s)
// A gap-only or diag-only blocks are possible.
//
// The block is frameshifting iff the difference in 
// projection-on-query vs projection-on-subject is not 
// a multiple of 3, or, equivalently, the projections
// are not congruent mod-3.
//
// (diag + q_ins) % 3  !=  (diag + s_ins ) % 3
//          q_ins % 3  !=  s_ins % 3;
struct SBlock
{
    int diag;  // matches and/or mismatches

    int q_ins; // gap(s) following the diag.
    int s_ins; // on query and subject respectively.

    SBlock() : 
        diag(0), 
        q_ins(0), 
        s_ins(0)
    {}

    bool HasGap() const
    {
        return q_ins > 0 || s_ins > 0;
    }

    bool IsFrameshifting() const
    {
        return (q_ins - s_ins) % 3 != 0;
    }

    void Add(const SBlock& other) 
    {
        diag  += other.diag;
        q_ins += other.q_ins;
        s_ins += other.s_ins;
    }

    // Return length of block's projection on
    // subject minimally truncated as to be
    // congruent mod-3 with projection on query.
    //
    // Note: in very rare cases where we need 
    // to truncate by more bases than we have
    // available, we'll extend by 1 or 2 bases
    // instead as to preserve frame.
    size_t GetFramePreservingSubjLen() const
    {
        const int q_len = diag + q_ins;
              int s_len = diag + s_ins;

        // Now truncate s_len by 0, 1, or 2 bases
        // until the length is congruent mod-3 with q_len.
        //
        // Note: s_len can become 0 or negative, e.g.
        // q_len = 2, and s_len = 1, (arising
        // from a degenerate case where we have an
        // exon consisting of a single-base diag and 
        // query-ins of 1)

        while( (q_len - s_len) % 3 ) {
            s_len--;
        }

        while(s_len < 0) {
            s_len += 3;
        }

        return s_len;
    }
};


struct SBlocks : public vector<SBlock>
{
public:

    SBlocks(const CSpliced_exon& spliced_exon)
    {
        SBlocks& blocks = *this;
        blocks.clear();

        bool last_is_diag = false;
        ITERATE(CSpliced_exon::TParts, it, spliced_exon.GetParts()) {
            const CSpliced_exon_chunk& chunk = **it;

            const int len = 
                chunk.IsMatch()       ? chunk.GetMatch()
              : chunk.IsMismatch()    ? chunk.GetMismatch()
              : chunk.IsDiag()        ? chunk.GetDiag()
              : chunk.IsGenomic_ins() ? chunk.GetGenomic_ins()
              : chunk.IsProduct_ins() ? chunk.GetProduct_ins()
              : 0;

            bool is_diag = chunk.IsMatch() 
                        || chunk.IsMismatch() 
                        || chunk.IsDiag();
            
            const bool start_new_block = 
                blocks.empty() ? true  // need non-empty blocks
              : len == 0       ? false // will be a no-op
              : !is_diag       ? false // gaps go into current block
              : last_is_diag   ? false // abutting diags get merged
              :                  true; // new non-abutting diag

            blocks.resize(blocks.size() + start_new_block);
                
            int& block_len = 
                  chunk.IsProduct_ins() ? blocks.back().q_ins
                : chunk.IsGenomic_ins() ? blocks.back().s_ins
                :                         blocks.back().diag;

            block_len += len;

            last_is_diag = is_diag;
        }

        // If spliced-exon has terminal subject-ins, then exon
        // projected with details will have missing bases at
        // the terminal. We need to modify the alignment
        // (i.e. the blocks-based representations thereof)
        // to insert anchor diags at the beginning and/or
        // end of the alignment, and truncate the alignment 
        // inward accordingly, such that exon terminals are 
        // exactly preserved despite terminal gaps.
        //
        // Note: this wouldn't be a problem if seq-locs with 
        // 0-base intervals were allowed, so we wolud have a 
        // 0-base interval in the beginning of packed-int, 
        // followed by int-loc separated by a gap, thus capturing
        // the original terminal gap while preserving extremes.

        while(   blocks.size() > 1 
              && blocks.front().diag < 3)
        {
            // The first block may be followed by a gap, and so
            // may need to be truncated by 1 or 2 bases to make
            // it frame-preserving, so we need it to be at least 3.
            if(blocks[1].diag > 0) {
                blocks[0].diag++;
                blocks[1].diag--;
            } else {
                // next block ran out
                blocks[1].Add(blocks[0]);
                blocks.erase(blocks.begin());
            }
        }

        if(   !blocks.empty()
           &&  blocks.back().diag > 0
           &&  blocks.back().HasGap())
        {
            // Handling the last block is similar, except
            // since the artificial diag will not have
            // gaps following it, we know that it will never
            // need truncation, and so we only need to borrow
            // one base from the alignment.
            blocks.back().diag--;
            blocks.push_back(SBlock());
            blocks.back().diag = 1;
        }

        // Fold abutting blocks if either of:
        //   * one of them does not have a diag 
        //     (essentially, absorbing the neigbor's gaps),
        //
        //   * gap between them (i.e. that of upstream's block) 
        //     is non-frameshifting.
        //
        //   * the downstream block's diag is 3 bases or less, except
        //     for the case of final diag that must be preserved.
        SBlocks::iterator dest = this->begin();
        ITERATE(SBlocks, it, *this) {
            if(it == dest) {
                ;
            } else if(   !it->diag 
                      || !dest->diag 
                      || !dest->IsFrameshifting()
                      || (   it->diag <= 3 
                          && &(*it) != &back()
                          && &(*it) != &front() ) )
            {
                dest->Add(*it);
            } else {
                ++dest;
                *dest = *it;
            }
        }
        EraseAfter(dest);
    }

private:
    void EraseAfter(SBlocks::iterator it)
    {
        if(it != this->end()) {
            ++it;
        }
        this->erase(it, this->end());
    }
};

static TSeqPos as_nucpos(const CProduct_pos& pos)
{
    const auto frame = // 0-based frame
        pos.IsProtpos()
     && pos.GetProtpos().IsSetFrame()
     && pos.GetProtpos().GetFrame() > 0 ? pos.GetProtpos().GetFrame() - 1
                                        : 0;

    return pos.IsNucpos() ? pos.GetNucpos() 
                          : pos.GetProtpos().GetAmin() * 3 + frame;
}


// Project exon to genomic coordinates, preserving discontinuities.
//
// aln_genomic* params are only used if not specified within exon
static CRef<CSeq_loc> ProjectExon_newlogic(
        const CSpliced_exon& exon, 
        const CSeq_id& aln_genomic_id, 
        ENa_strand aln_genomic_strand)  
{
    CRef<CSeq_loc> exon_loc(new CSeq_loc(CSeq_loc::e_Packed_int));

    const CSeq_id& genomic_id = 
            exon.IsSetGenomic_id() ? exon.GetGenomic_id() 
                                   : aln_genomic_id;

    const ENa_strand genomic_strand = 
            exon.IsSetGenomic_strand() ? exon.GetGenomic_strand() 
                                       : aln_genomic_strand;

    //Don't have exon details - create based on exon boundaries and return.
    if(!exon.IsSetParts()) {
        exon_loc->SetInt().SetId().Assign( genomic_id);
        exon_loc->SetInt().SetStrand(      genomic_strand);
        exon_loc->SetInt().SetFrom(        exon.GetGenomic_start());
        exon_loc->SetInt().SetTo(          exon.GetGenomic_end());
        return exon_loc;
    } else {
        exon_loc->SetPacked_int();
    }

    const int genomic_sign = (genomic_strand == eNa_strand_minus) ? -1 : 1;

    const TSeqPos exon_bio_start_pos = 
        genomic_sign > 0 ? exon.GetGenomic_start() 
                         : exon.GetGenomic_end(); 

    // chunk's start in exon-local coords
    size_t exon_relative_subj_start = 0;

    const SBlocks blocks(exon);
    ITERATE(SBlocks, it, blocks) {

        const TSeqPos bio_start = exon_bio_start_pos 
                                + (exon_relative_subj_start * genomic_sign);

        const size_t len = it->GetFramePreservingSubjLen();

        const TSeqPos bio_stop = bio_start + ((len - 1) * genomic_sign);
            // -1 because stop is inclusive
 
        CRef<CSeq_interval> chunk(new CSeq_interval);
        chunk->SetId().Assign(genomic_id);
        chunk->SetStrand( genomic_strand);
        chunk->SetFrom(   genomic_sign > 0 ? bio_start : bio_stop);
        chunk->SetTo(     genomic_sign > 0 ? bio_stop : bio_start);

        if(len > 0) {
            exon_loc->SetPacked_int().Set().push_back(chunk);
        }
        
        exon_relative_subj_start += it->diag + it->s_ins;
    }

    NTweakExon::SubsumeMicroIntervals(exon_loc->SetPacked_int());
    NTweakExon::CollapseNonframeshiftting(exon_loc->SetPacked_int());

    try { 
        // Validate 
        const size_t exon_product_len = 
            as_nucpos(exon.GetProduct_end())
          - as_nucpos(exon.GetProduct_start()) + 1;

        const size_t loc_len = sequence::GetLength(*exon_loc, NULL);

        if(loc_len % 3 != exon_product_len % 3) {
            NCBI_THROW(CException, eUnknown, 
                       "Logic error - frame not preserved");
        }

        if(   exon.GetGenomic_start() 
           != exon_loc->GetStart(eExtreme_Positional)) 
        {
            NCBI_USER_THROW("Change in positional-starts");
        }

        if(exon.GetGenomic_end() 
           != exon_loc->GetStop(eExtreme_Positional)) 
        {
            NCBI_USER_THROW("Change in positional-stops");
        }

    } catch (CException& e) {
        NcbiCerr << MSerial_AsnText << exon;
        NcbiCerr << MSerial_AsnText << *exon_loc;
        NCBI_RETHROW_SAME(e, "Invalid result");
    }


    return exon_loc;
}

// GP-17626
static CRef<CSeq_loc> ProjectExon(
        const CSpliced_exon& exon, 
        const CSeq_id& aln_genomic_id, 
        ENa_strand aln_genomic_strand,
        bool convert_overlaps)  
{
    CRef<CSeq_loc> exon_loc;
   
    try {
        exon_loc = ProjectExon_oldlogic(exon, aln_genomic_id, aln_genomic_strand, convert_overlaps);
    } catch(CException&) {
        exon_loc = ProjectExon_newlogic(exon, aln_genomic_id, aln_genomic_strand);

        ERR_POST(Warning << " Project-exon logic failed. Using new logic: " 
                         << NTweakExon::AsString(exon_loc->GetPacked_int()) << "\n");
    }

    return exon_loc;
}


/// Create an exon with the structure consisting of 
/// two diags extending inwards from the exon terminals
/// with a single gap of required length in the middle. 
/// This is used for projecting cds-exons consisting 
/// entirely of gaps (see ProjectExons)
CRef<CSpliced_exon> CollapseExonStructure(const CSpliced_exon& orig_exon)
{
    CRef<CSpliced_exon> exon(SerialClone(orig_exon));
    TSeqPos query_range = as_nucpos(exon->GetProduct_end())
                        - as_nucpos(exon->GetProduct_start()) + 1;
    TSeqPos subject_range = exon->GetGenomic_end() - exon->GetGenomic_start() + 1;
    TSeqPos min_range = min(query_range, subject_range);
    TSeqPos max_range = max(query_range, subject_range);
  
    CRef<CSpliced_exon_chunk> diag1(new CSpliced_exon_chunk);
    CRef<CSpliced_exon_chunk> gap_chunk(new CSpliced_exon_chunk);
    CRef<CSpliced_exon_chunk> diag2(new CSpliced_exon_chunk);
    diag1->SetDiag(min_range / 2);
    diag2->SetDiag(min_range - diag1->GetDiag());
    if(max_range == subject_range) {
        gap_chunk->SetGenomic_ins(max_range - min_range);
    } else {
        gap_chunk->SetProduct_ins(max_range - min_range);
    }
    exon->SetParts().clear();
    exon->SetParts().push_back(diag1);
    exon->SetParts().push_back(gap_chunk);
    exon->SetParts().push_back(diag2);
    return exon;
}

#if 0
/*
This logic is now disabled, and CDS-exons are computed based on 
genomic-cds overlap with projected mRNA feature.
see: TruncateToCDS(...)

Caveat-1 is no longer relevant because overlaps are converted to
gaps in TweakExon.

Caveat-2 is dealt with by projecting and collapsing each product-cds-range
individually, and then intersecting rna-loc with each range.
see: ProjectAndCollapseCDS(...)

Caveat-3 is not applicable in the new logic.

Caveat-4 is no longer relevant because the rules were restricted so
that we're no longer obligated to preserve the product length during
mapping, only the frame. According to the current rule it would be 
unexpected for the projected product or cds loc to extend past the 
terminals of "naively-mapped" exons.
*/


// Creating exon-loc for CDS:
//
// Caveat-1:
// A naive way to project CDS would be to take the genomic cds-range 
// and intersect with projected RNA.
// Such approach is clear, but will not work when the genomic cds boundary 
// is in the overlap of the exon chunks (in product-ins).
// Instead, we'll truncate the original spliced-seg down to product-CDS 
// and will generate exons-loc the same way as for RNA.
//
// Caveat-2: a CDS on product can itself have discontinuities 
// (e.g. ribosomal slippage), and simply projecting truncated-to-cds alignment
// will not capture these. Instead, we'll take each product-cds chunk, 
// tructate alignment to that, project, and combine the results.
// During the combination of results we'll have to combine sublocs pertaining
// to same exon in the same subloc of the container mix.
//
// Caveat-3: currently Seq-loc-Mapper has a bug, such that truncating 
// an alignment to CDS yields wrong result for multi-exon cases (CXX-3724), 
// so to work-around that we'll create a single-exon alignment for each exon. 
// Additionally, doing it exon-by-exon makes it easy to
// combine projected result for each exon (from multiple cds sublocs).
//  
// Caveat-4: Remapping may produce gap-only exons, which would ordinarily yield
// no genomic projection counterpart, but in the context of 
// discontinuity-preservation we'll need to calculate genomic projection "manually".
CRef<CSeq_loc> ProjectCDSExon(
        const CSeq_align& spliced_aln,
        const CSpliced_exon& spliced_exon,
        const CSeq_loc& product_cds_loc,
        bool convert_overlaps)
{
    CRef<CSeq_align> exon_aln(SerialClone(spliced_aln)); 
    exon_aln->ResetScore();
    exon_aln->ResetExt();

    //Create alignment to represent only the current exon
    exon_aln->SetSegs().SetSpliced().SetExons().clear();
    exon_aln->SetSegs().SetSpliced().SetExons().push_back(
            CRef<CSpliced_exon>(SerialClone(spliced_exon)));

    CRef<CSeq_loc> query_exon_loc = exon_aln->CreateRowSeq_loc(0);

    CRef<CSeq_loc> exon_loc(new CSeq_loc(CSeq_loc::e_Packed_int));

    for(CSeq_loc_CI ci(product_cds_loc, 
                       CSeq_loc_CI::eEmpty_Skip, 
                       CSeq_loc_CI::eOrder_Biological); ci; ++ci) 
    {
        CConstRef<CSeq_loc> cds_subloc = ci.GetRangeAsSeq_loc();
        
        if(sequence::eNoOverlap == sequence::Compare(
               *query_exon_loc, 
               *cds_subloc,
               NULL, 
               sequence::fCompareOverlapping)) 
        {
            // exon does not overlap the CDS interval 
            // (i.e. UTR-only, or, in rare case of translational-frameshifts, 
            // not specific to this cds-chunk)
            continue;
        }


        // truncate the exon-alignment to the query-cds-subloc
        CRef<CSeq_loc_Mapper> mapper(
                new CSeq_loc_Mapper(*cds_subloc, *cds_subloc, NULL));
        mapper->SetTrimSplicedSeg(false);

        CRef<CSeq_align> truncated_exon_aln;
        try {
            truncated_exon_aln = mapper->Map(*exon_aln);
        } catch (CAnnotMapperException& e) {
            // It used to be the case that the mapper would return an empty alignment,
            // but in GP-11467 it was discovered that it can also throw
            // "Mapping resulted in an empty alignment, can not initialize Seq-align."
            if(e.GetErrCode() == CAnnotMapperException::eBadAlignment) {
                truncated_exon_aln.Reset(new CSeq_align);
                truncated_exon_aln->Assign(*exon_aln);
                truncated_exon_aln->SetSegs().SetSpliced().SetExons().clear();
            } else {
                NcbiCerr << MSerial_AsnText << *cds_subloc;
                NcbiCerr << MSerial_AsnText << *exon_aln;
                NCBI_RETHROW_SAME(e, "Can't truncate alignment to CDS");
            }
        }
       
#if 0 
        NcbiCerr << MSerial_AsnText << *cds_subloc;
        NcbiCerr << MSerial_AsnText << *exon_aln;
        NcbiCerr << MSerial_AsnText << *truncated_exon_aln;
        NcbiCerr << "\n";
#endif

        if(truncated_exon_aln->GetSegs().GetSpliced()
                              .GetExons().empty()) 
        {
            // NcbiCerr << "gap-only cds-exon: " 
            //      << MSerial_AsnText <<spliced_aln;
            //
            // This is a rare case where the exon overlaps the CDS, 
            // but truncating the alignment to the CDS
            // produced empty alignment - how can this happen? 
            // This is the case where an exon has a product-ins
            // abutting the exon terminal, and the CDS part does 
            // not extend past the gap, such that the result of
            // truncation is a gap-only alignment. 
            // To deal with this we'll take a chunk of required length
            // starting at genomic exon boundary (i.e. as if the exon 
            // structure abutted a diag rather than a gap).
            //
            // We'll do this by ignoring the exon structure, and 
            // instead create a dummy exon consisting of two diags 
            // extending from the exon terminals with a gap of 
            // necessary length in the middle.
            //
            // Note: The result is the same as if the seq-loc-mapper 
            // preserved the gap-only alignment instead of 
            // throwing away the exon, which would result in 
            // |product-ins| nucleotides being translated from the
            // genomic exon boundary.
            CRef<CSpliced_exon> collapsed_exon = 
                CollapseExonStructure(
                        *exon_aln->SetSegs().SetSpliced().SetExons().front());   

            exon_aln->SetSegs().SetSpliced().SetExons().front() = collapsed_exon;
            truncated_exon_aln = mapper->Map(*exon_aln);

            if(truncated_exon_aln->GetSegs().GetSpliced()
                                  .GetExons().empty()) 
            {
                continue; 
                //theoretically this shouldn't happen, 
                //but we can't proceed otherwise
            }            
        }

        CRef<CSeq_loc> exon_subloc = ProjectExon(
                *truncated_exon_aln->GetSegs().GetSpliced().GetExons().front(),
                spliced_aln.GetSeq_id(1),
                spliced_aln.GetSeqStrand(1));


#if 0
        // GP-15635 
        // This is wrong, because this will add partialness to every cds-exon.
        // Instead, the caller will make sure that the partialness is 
        // properly inherited from cds-loc for the aggregate exons-loc
        // (see GetTerminalPartialness(...)
        AugmentPartialness(
                *exon_subloc, 
                T53Partialness(
                    cds_subloc->IsPartialStart(eExtreme_Biological),
                    cds_subloc->IsPartialStop(eExtreme_Biological)));
#endif

        exon_loc->SetPacked_int().Set().insert(
                exon_loc->SetPacked_int().Set().end(),
                exon_subloc->SetPacked_int().Set().begin(),
                exon_subloc->SetPacked_int().Set().end());

    }
    return exon_loc;
}
#endif


// for each product-cds-subloc:
//     project to genome coords and collapse to single-range.
// Output: translational-frameshift-preserving-collapsed-CDS
CRef<CPacked_seqint> ProjectAndCollapseCDS(
    const CSeq_align& spliced_aln,
    CConstRef<CSeq_loc> product_cds_loc)
{
    if(!product_cds_loc) {
        return CRef<CPacked_seqint>(NULL);
    }

    CRef<CPacked_seqint> out(new CPacked_seqint);

    CRef<CSeq_loc_Mapper> mapper(
            new CSeq_loc_Mapper(spliced_aln, 1, NULL));

    for(CSeq_loc_CI ci(*product_cds_loc, 
                       CSeq_loc_CI::eEmpty_Skip, 
                       CSeq_loc_CI::eOrder_Biological); ci; ++ci) 
    {
        CConstRef<CSeq_loc> cds_subloc = ci.GetRangeAsSeq_loc();
        CConstRef<CSeq_loc> mapped_cds_subloc = mapper->Map(*cds_subloc);
        CRef<CSeq_loc> mapped_collapsed_cds_subloc = 
            sequence::Seq_loc_Merge(
                    *mapped_cds_subloc, 
                    CSeq_loc::fMerge_SingleRange, 
                    NULL);

        if(mapped_collapsed_cds_subloc->IsNull()) {
            ;
        } else if(mapped_collapsed_cds_subloc->IsInt()) {
            CRef<CSeq_interval> seqint(
                    &mapped_collapsed_cds_subloc->SetInt());
            out->Set().push_back(seqint); 
        } else {
            NCBI_USER_THROW("Expected seqint or null-loc");
        }
    }
    return out;
}

void TransferPartialness(
        const CSeq_loc& src,
        CSeq_loc& dest)
{
    if(    !src.GetId() 
        || !dest.GetId() 
        || !src.GetId()->Equals(*dest.GetId())
        || src.GetStrand() != dest.GetStrand() )
    {
        return;
    }
    
    const objects::ESeqLocExtremes ext = eExtreme_Biological;
    const bool same_start = src.GetStart(ext) == dest.GetStart(ext);
    const bool same_stop  = src.GetStop(ext)  == dest.GetStop(ext); 

    if(same_start && src.IsPartialStart(ext))   
                   dest.SetPartialStart(true, ext); 

    if(same_start && src.IsTruncatedStart(ext)) 
                   dest.SetTruncatedStart(true, ext); 

    if(same_stop && src.IsPartialStop(ext))    
                   dest.SetPartialStop(true, ext); 

    if(same_stop && src.IsTruncatedStop(ext))  
                  dest.SetTruncatedStop(true, ext); 
}


CRef<CSeq_loc> TruncateToCDS(
    CRef<CSeq_loc> detailed_rna_exon, //mix-of-(int-or-packed-seqint)
    CConstRef<CPacked_seqint> collapsed_genomic_cds)
{
    if(!collapsed_genomic_cds) {
        return detailed_rna_exon;
    }

    CRef<CSeq_loc> out_loc(new CSeq_loc(CSeq_loc::e_Mix));

    ITERATE(CPacked_seqint::Tdata, it, collapsed_genomic_cds->Get()) {
        CSeq_loc cds_subrange;
        cds_subrange.SetInt(const_cast<CSeq_interval&>(**it));

        CRef<CSeq_loc> cds_exon = 
            detailed_rna_exon->Intersect(cds_subrange, 0, NULL);

        if(cds_exon->IsNull()) {
            continue;
        }

        TransferPartialness(cds_subrange, *cds_exon);

        out_loc->SetMix().Set().push_back(cds_exon);
    }

    out_loc->ChangeToPackedInt();
    return out_loc;
}



CRef<CSeq_loc> ProjectExons(const CSeq_align& spliced_aln, 
                            CConstRef<CSeq_loc> product_cds_loc,
                            bool convert_overlaps,
                            size_t unaligned_ends_partialness_thr = 0)
{
    CRef<CSeq_loc> exons_loc(new CSeq_loc(CSeq_loc::e_Mix));

    CConstRef<CPacked_seqint> genomic_collapsed_cds = 
        ProjectAndCollapseCDS(spliced_aln, product_cds_loc);

    ITERATE(CSpliced_seg::TExons, it, 
            spliced_aln.GetSegs().GetSpliced().GetExons()) 
    {
        const CSpliced_exon& spliced_exon = **it;
       
        CRef<CSeq_loc> exon_loc = 
            TruncateToCDS(
                ProjectExon(
                      spliced_exon, 
                      spliced_aln.GetSeq_id(1), 
                      spliced_aln.GetSeqStrand(1),
                      convert_overlaps),
                genomic_collapsed_cds);

        const T53Partialness partialness =
            GetExonPartialness(spliced_aln, spliced_exon);

        if(!product_cds_loc) {
            AugmentPartialness(*exon_loc, partialness);
        } else if(exon_loc->GetId()) {
            // note: if no seq-id, GetStart/GetStop will throw - GP-15887
            //
            // GP-15635/case-(3,4):
            // Inherit partialness only if the CDS
            // mapped up to the exon's terminal
            bool start_partial = partialness.first;
            bool stop_partial  = partialness.second;

            // convert to positional
            if(spliced_aln.GetSeqStrand(1) == eNa_strand_minus) {
                swap(start_partial, stop_partial);
            }

            if(start_partial
               && sequence::GetStart(*exon_loc, NULL) 
                  == spliced_exon.GetGenomic_start())
            {
                exon_loc->SetPartialStart(true, eExtreme_Positional);
            }
            
            if(stop_partial
               && sequence::GetStop(*exon_loc, NULL) 
                  == spliced_exon.GetGenomic_end())
            {
                exon_loc->SetPartialStop(true, eExtreme_Positional);
            }
        }

        exons_loc->SetMix().Set().push_back(exon_loc);
    }

    Canonicalize(*exons_loc);

    AugmentPartialness(
            *exons_loc, 
            GetTerminalPartialness(
                spliced_aln, 
                product_cds_loc, 
                unaligned_ends_partialness_thr));

    return exons_loc;
}


/// Precondition: input loc is discontinuity-preserving RNA loc
/// Postcontition: adjacent packed-ints having the discontinuity 
/// between them entirely outside of cds-range are merged into single interval.
CRef<CSeq_loc> CollapseDiscontinuitiesInUTR(
        const CSeq_loc& loc, 
        TSeqPos cds_start, 
        TSeqPos cds_stop)
{
    CRef<CSeq_loc> collapsed_loc(new CSeq_loc(CSeq_loc::e_Null));
    if(loc.IsMix()) {
        //each subloc is an exon - recurse on each.
        collapsed_loc->SetMix();
        ITERATE(CSeq_loc::TMix::Tdata, it, loc.GetMix().Get()) {
            
            CRef<CSeq_loc> collapsed_exon_loc = 
                CollapseDiscontinuitiesInUTR(**it, cds_start, cds_stop);

            collapsed_loc->SetMix().Set().push_back(collapsed_exon_loc);
        }

    } else if(loc.IsPacked_int()) {

        //each subloc is a chunk in an exon 
        //- will merge compatible adjacent chunks iff outside of CDS
        collapsed_loc->SetPacked_int();
        ITERATE(CPacked_seqint::Tdata, it, loc.GetPacked_int().Get()) {
            const CSeq_interval& interval = **it;
            
            if(collapsed_loc->GetPacked_int().Get().empty()) {
                collapsed_loc->SetPacked_int().Set().push_back(
                        CRef<CSeq_interval>(SerialClone(interval)));
                continue;
            }
            
            CSeq_interval& last_interval = 
                *collapsed_loc->SetPacked_int().Set().back();
       
            // We can collapse intervals iff the discontinuity 
            // (overlap or gap) between them lies outside of the CDS. 
            // Equivalently, the count of interval terminals overlapping CDS
            // being at most 1 is necessary and sufficient 
            // (allowing for one of the intervals to be partially in the CDS)
            
            size_t count_terminals_within_cds =
                (last_interval.GetFrom() >= cds_start && last_interval.GetFrom() <= cds_stop ? 1 : 0)
              + (     interval.GetFrom() >= cds_start &&      interval.GetFrom() <= cds_stop ? 1 : 0)
              + (last_interval.GetTo()   >= cds_start && last_interval.GetTo()   <= cds_stop ? 1 : 0)
              + (     interval.GetTo()   >= cds_start &&      interval.GetTo()   <= cds_stop ? 1 : 0);
 
            if(   count_terminals_within_cds <= 1
               && last_interval.GetStrand() == interval.GetStrand()
               && last_interval.GetId().Equals(interval.GetId()))
            {
                CSeq_loc loc1, loc2;
                loc1.SetInt(last_interval);
                loc2.SetInt(const_cast<CSeq_interval&>(interval));
                CRef<CSeq_loc> loc3 = sequence::Seq_loc_Add(
                        loc1, loc2, CSeq_loc::fMerge_SingleRange, NULL);
                last_interval.Assign(loc3->GetInt());
            } else {
#if 0
                NcbiCerr << "Retaining UTR indel: "
                         << "cds: " << cds_start << ".." << cds_stop << "; "
                         << "terminals_in_cds: " << count_terminals_within_cds << "; "
                         << "last: " << MSerial_AsnText << last_interval
                         << "this: " << MSerial_AsnText << interval;
#endif
                collapsed_loc->SetPacked_int().Set().push_back(
                        CRef<CSeq_interval>(SerialClone(interval)));
            }
        }
        
        // even if the original was canonicalized, 
        // we may have collapsed packed-int sublocs such 
        // that there's only one remaining,
        // so need to recanonicalize
        Canonicalize(*collapsed_loc);
    } else {
        collapsed_loc->Assign(loc);
    }

    return collapsed_loc;
}


// GP-38765
// Given projected rna orcds-loc, shift the micro-intron downstream|upstream
// for start|stop codon if the terminal chunk is less than 3bp (partial codon).
static CRef<CSeq_loc> PreserveTerminalCodons(CRef<CSeq_loc> exons_loc, TSeqPos cds_start, TSeqPos cds_stop)
{
    if (cds_start == kInvalidSeqPos && cds_stop == kInvalidSeqPos) {
        return exons_loc;
    }

    bool is_minus_strand = exons_loc->GetStrand() == eNa_strand_minus;

    // convert to biological
    if (exons_loc->GetStrand() == eNa_strand_minus && cds_start < cds_stop) {
        std::swap(cds_start, cds_stop);
    }

    const auto orig_len = sequence::GetLength(*exons_loc, NULL);
    (void)orig_len;

    CSeq_loc_mix loc_mix{};
    loc_mix.Set().push_back(exons_loc);
    auto& exons = exons_loc->IsMix() ? exons_loc->SetMix().Set() : loc_mix.Set();
    
    for (auto it = exons.begin(); it != exons.end(); ++it) {
        CSeq_loc& exon = **it;
        if (!exon.IsPacked_int() || exon.GetPacked_int().Get().size() < 2) {
            continue;
        }
        // dealing with multipart exon (with micro-introns)

        auto& chunks = exon.SetPacked_int().Set();

        for (auto it = chunks.begin(); it != chunks.end(); ++it) {
            auto      ivl = *it;
            auto prev_ivl = it == chunks.begin()          ? CRef<CSeq_interval>{} : *std::prev(it);
            auto next_ivl = it == std::prev(chunks.end()) ? CRef<CSeq_interval>{} : *std::next(it);

            auto is_overlapping = [&](TSeqPos pos)
            {
                return ivl->GetFrom() <= pos && pos <= ivl->GetTo();
            };

            if (   is_overlapping(cds_start)                               // 1st bp of start-codon
                && !is_overlapping(cds_start + (is_minus_strand ? -2 : 2)) // 3rd bp of start codon
                && next_ivl
                && next_ivl->GetLength() > 3)
            {
                NTweakExon::AdjustBioStop(*ivl, 3);
                NTweakExon::AdjustBioStart(*next_ivl, 3);
            }

            if (   is_overlapping(cds_stop)                               // 3rd bp of stop-codon
                && !is_overlapping(cds_stop - (is_minus_strand ? -2 : 2)) // 1st bp of stop-codon
                && prev_ivl
                && prev_ivl->GetLength() > 3)
            {
                NTweakExon::AdjustBioStop(*prev_ivl, -3);
                NTweakExon::AdjustBioStart(*ivl, -3);
            }
        }
    }

    const auto final_len = sequence::GetLength(*exons_loc, NULL);
    (void)final_len;
    _ASSERT(orig_len == final_len);

    return exons_loc;
}

///////////////////////////////////////////////////////////////////////////////

CRef<CSeq_loc> CFeatureGenerator::s_ProjectRNA(
        const CSeq_align& spliced_aln, 
        CConstRef<CSeq_loc> product_cds_loc,
        size_t unaligned_ends_partialness_thr)
{
    CRef<CSeq_loc> projected_rna_loc = 
        ProjectExons(
                spliced_aln,
                CConstRef<CSeq_loc>(NULL),
                true,
                unaligned_ends_partialness_thr);

    TSeqPos cds_start(kInvalidSeqPos), 
            cds_stop(kInvalidSeqPos);

    if(product_cds_loc) {
        CRef<CSeq_loc_Mapper> mapper(
                new CSeq_loc_Mapper(spliced_aln, 1, NULL));
        mapper->SetTrimSplicedSeg(false);

        CRef<CSeq_loc> genomic_cds_range = mapper->Map(*product_cds_loc);

        genomic_cds_range =
            sequence::Seq_loc_Merge(
                    *genomic_cds_range, 
                    CSeq_loc::fMerge_SingleRange, 
                    NULL);

        cds_start = genomic_cds_range->GetStart(eExtreme_Positional);
        cds_stop  = genomic_cds_range->GetStop(eExtreme_Positional);
    }

    // note, if there's no product-cds-loc, 
    // this will collapse discontinuities in every exon
    return 
        PreserveTerminalCodons(
            CollapseDiscontinuitiesInUTR(
                *projected_rna_loc, 
                cds_start, cds_stop),
            cds_start, cds_stop);
}

CRef<CSeq_loc> CFeatureGenerator::s_ProjectCDS(
        const CSeq_align& spliced_aln,
        const CSeq_loc& product_cds_loc,
        bool convert_overlaps)
{
    auto loc = ProjectExons(
        spliced_aln, 
        CConstRef<CSeq_loc>(&product_cds_loc),
        convert_overlaps);

    return PreserveTerminalCodons(
        loc,
        loc->GetStart(eExtreme_Positional),
        loc->GetStop(eExtreme_Positional));
}


///////////////////////////////////////////////////////////////////////////////

#if 0

todo: move to unit-test

void CollapseMatches(CSeq_align& spliced_aln)
{
    NON_CONST_ITERATE(CSpliced_seg::TExons, it, spliced_aln.SetSegs().SetSpliced().SetExons()) {
        CSpliced_exon& spliced_exon = **it;
        CRef<CSpliced_exon> se(new CSpliced_exon());
        se->Assign(spliced_exon);

        spliced_exon.SetParts().clear();
        ITERATE(CSpliced_exon::TParts, it2, se->GetParts()) {
            const CSpliced_exon_chunk& chunk = **it2;

                int len = chunk.IsMatch()       ? chunk.GetMatch()
                              : chunk.IsMismatch()    ? chunk.GetMismatch()
                              : chunk.IsDiag()        ? chunk.GetDiag()
                              : chunk.IsGenomic_ins() ? chunk.GetGenomic_ins()
                              : chunk.IsProduct_ins() ? chunk.GetProduct_ins()
                              : 0;

            bool current_is_diag = chunk.IsMatch() || chunk.IsDiag() || chunk.IsMismatch();

            if(spliced_exon.GetParts().size() > 0 && spliced_exon.GetParts().back()->IsDiag() && current_is_diag) {
                spliced_exon.SetParts().back()->SetDiag() += len;
            } else {
                CRef<CSpliced_exon_chunk> chunk2(new CSpliced_exon_chunk);
                chunk2->Assign(chunk);
                if(current_is_diag) {
                    chunk2->SetDiag(len);
                }
                spliced_exon.SetParts().push_back(chunk2);
            }
        }
    }
}

/*
 fp_cds            := Create Frame-preserving cds loc.
 covered_cds       := Intersect CDS on product with the alignment's query-loc.
 query_seq         := Instantiate covered_cds sequence.
 genomic_seq       := Instantiate fp_cds sequence. 
 ASSERT: query_seq and genomic_seq are of the same length and the count of matches is at least
         as in original alignment truncated to query-cds
*/
bool CFeatureGenerator::TestProjectExons(const CSeq_align& aln2)
{
    CScope& scope = *m_impl->m_scope;
    CRef<CSeq_align> aln_ref(new CSeq_align);
    aln_ref->Assign(aln2);
    CollapseMatches(*aln_ref);
    const CSeq_align& aln = *aln_ref;

    CBioseq_Handle product_bsh = scope.GetBioseqHandle(aln.GetSeq_id(0));
    CRef<CSeq_loc> query_loc = aln.CreateRowSeq_loc(0);

    bool all_ok = false; //for every CDS on query (normally just one)
    for(CFeat_CI ci(product_bsh, SAnnotSelector(CSeqFeatData::e_Cdregion)); ci; ++ci) {
        bool this_ok = true;
        const CMappedFeat& mf= *ci;

        //CRef<CSeq_loc> covered_cds = query_loc->Intersect(mf.GetLocation(), 0, NULL);
        //Note: this intersect is incorrect, at it will not represent overlaps within cds-loc
        //The intersect below is correct.
        CRef<CSeq_loc> covered_cds = mf.GetLocation().Intersect(*query_loc, 0, NULL);

        if(covered_cds->IsNull()) {
            continue;
        }

        static const size_t allowed_unaligned_ends_len = 6;
        CRef<CSeq_loc> rna_loc = ProjectRNA(aln, CConstRef<CSeq_loc>(&mf.GetLocation()), allowed_unaligned_ends_len);
        CRef<CSeq_loc> cds_loc = ProjectCDS(aln, mf.GetLocation());

        CSeqVector query_sv(*covered_cds, scope, CBioseq_Handle::eCoding_Iupac);
        CSeqVector subject_sv(*cds_loc, scope, CBioseq_Handle::eCoding_Iupac);

        if(query_sv.size() != subject_sv.size()) {
            ERR_POST(Error << "In alignment of " << aln.GetSeq_id(0).AsFastaString() 
                           << "->" << aln.GetSeq_id(1).AsFastaString() << ": " 
                           << "|query-cds truncated to aln|=" << query_sv.size()
                           << "; |projected-cds|=" << subject_sv.size());
            this_ok = false;
        } else {
            //we expect the count of matches in seq-vectors to be equal or greater to the
            //count of matches in the alignment truncated to CDS 
            //(accounting for matches in the alignment, plus random matches in overlaps 
            //corresponding to product-insertions)

            size_t aln_cds_matches(0);
            {{
                CRef<CSeq_loc_Mapper> mapper(new CSeq_loc_Mapper(mf.GetLocation(), mf.GetLocation(), NULL));
                CRef<CSeq_align> cds_aln = mapper->Map(aln2);
                for(CTypeConstIterator<CSpliced_exon_chunk> it(Begin(*cds_aln)); it; ++it) {
                    const CSpliced_exon_chunk& chunk = *it;
                    if(chunk.IsMatch()) {
                        aln_cds_matches += chunk.GetMatch();
                    }
                }
            }}

            size_t seq_cds_matches(0);
            for(size_t i = 0; i < query_sv.size(); i++) {
                seq_cds_matches += (query_sv[i] == subject_sv[i] ? 1 : 0);
            }

            if(seq_cds_matches < aln_cds_matches) {
                ERR_POST(Error << "In alignment of " << aln.GetSeq_id(0).AsFastaString() 
                               << "->" << aln.GetSeq_id(1).AsFastaString() << ": " 
                               << aln_cds_matches << " matches in alignment truncated to CDS, but only " 
                               << seq_cds_matches << " matches in seq-vector");
                this_ok = false;
            }
        }
#if 0
        //for debugging
        if(!ok) {
            NcbiCerr << MSerial_AsnText << aln << "aln(0): " << MSerial_AsnText << *query_loc << "cds(0): " << MSerial_AsnText << mf.GetLocation() << "aln-cds(0): " << MSerial_AsnText <<  *covered_cds << MSerial_AsnText << *rna_loc << sequence::GetLength(*rna_loc, NULL) << "\n" << MSerial_AsnText << *cds_loc;
        }
#endif
        all_ok = all_ok & this_ok;
    }

    return all_ok;
}
#endif

END_NCBI_SCOPE
