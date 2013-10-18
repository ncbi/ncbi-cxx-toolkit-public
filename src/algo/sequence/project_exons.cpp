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



/// Recursively convert empty container-locs to null-locs, drop null sublocs from containers, and unwrap singleton containers
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

/// ::first and ::second indicate partialness for of a loc or an exon, 5' and 3' end respectively.
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
        if(!prev_exon) {
            prev_exon = current_exon;
            continue;
        }
        
        //gap between exons. Determine which exon is partial based on consensus splice
        if(!AreAbuttingOnProduct(*prev_exon, *current_exon)) {
            bool is_consensus_donor = prev_exon->IsSetDonor_after_exon() ?
                prev_exon->GetDonor_after_exon().GetBases() == "GT" : false;

            bool is_consensus_acceptor = current_exon->IsSetAcceptor_before_exon() ? 
                current_exon->GetAcceptor_before_exon().GetBases() == "AG" : false;

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
        }

        prev_exon = current_exon;
    }

    return T53Partialness(is_5p_partial, is_3p_partial);
}

size_t GetUnalignedLength_3p(const CSeq_align& spliced_aln)
{
    return spliced_aln.GetSegs().GetSpliced().IsSetPoly_a()         ? 0
         : spliced_aln.GetSeqStrand(0) == eNa_strand_minus          ? spliced_aln.GetSeqStart(0)
         : spliced_aln.GetSegs().GetSpliced().IsSetProduct_length() ? spliced_aln.GetSegs().GetSpliced().GetProduct_length() 
                                                                      - spliced_aln.GetSeqStop(0) - 1
         : 0;
}

size_t GetUnalignedLength_5p(const CSeq_align& spliced_aln)
{
    return spliced_aln.GetSeqStrand(0) != eNa_strand_minus          ? spliced_aln.GetSeqStart(0)
         : spliced_aln.GetSegs().GetSpliced().IsSetProduct_length() ? spliced_aln.GetSegs().GetSpliced().GetProduct_length() 
                                                                      - spliced_aln.GetSeqStop(0) - 1
         : 0;
}

/// Return whether 5' and/or 3' end of exons-loc is partial
/// based on unaligned tails in case of RNA,
///       or overlap of product-cds-loc with unaligned tails, in case of CDS.
T53Partialness GetTerminalPartialness(const CSeq_align& spliced_aln,
                                      CConstRef<CSeq_loc> product_cds_loc,
                                      size_t unaligned_ends_partialness_thr)
{
    T53Partialness partialness(false, false);
    if(!product_cds_loc) {
        //For RNA set partialness based on whether the unaligned ends are longer than allow_terminal_unaligned_bases
        partialness.first = GetUnalignedLength_5p(spliced_aln) > unaligned_ends_partialness_thr;
        partialness.second = GetUnalignedLength_3p(spliced_aln) > unaligned_ends_partialness_thr;
    } else {
        //cds-exons 5p/3p-terminal partialness is based on whether the product-cds-loc extends past the alignment
        bool left_partial = product_cds_loc->GetStart(eExtreme_Positional) < spliced_aln.GetSeqStart(0);
        bool right_partial = product_cds_loc->GetStop(eExtreme_Positional) > spliced_aln.GetSeqStop(0);

        partialness.first =  (left_partial && spliced_aln.GetSeqStrand(0) == eNa_strand_plus)
                          || (right_partial && spliced_aln.GetSeqStrand(0) == eNa_strand_minus);

        partialness.second = (left_partial && spliced_aln.GetSeqStrand(0) == eNa_strand_minus)
                          || (right_partial && spliced_aln.GetSeqStrand(0) == eNa_strand_plus);
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


/// Project exon to genomic coordinates, preserving discontinuities.
CRef<CSeq_loc> ProjectExon(const CSpliced_exon& spliced_exon, 
                           const CSeq_id& aln_genomic_id,  //of the parent alignment (if not specified in spliced_exon)
                           ENa_strand aln_genomic_strand)  //of the parent alignment (if not specified in spliced_exon)
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
        //Each element is an exon chunk comprised of alignment diag (match or mismatch run) and abutting downstream gaps.
        //::first  is diag+query_gap  , corresponding to the transcribed chunk length. 
        //::second is diag+subject_gap, corresponding to distance to the start of the next chunk.

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
            (chunk.IsProduct_ins() ? exon_structure.back().first : exon_structure.back().second) += len;
        }
        last_is_diag = is_diag;
    }

    //make the subject values cumulative (i.e. relative to the exon boundary, rather than neighboring chunk)
    //After this, the biological start of a chunk relative to the exon-start is ::second of the previous element
    NON_CONST_ITERATE(TExonStructure, it, exon_structure) {
        if(it != exon_structure.begin()) {
            it->second += (it-1)->second;
        }   
    }

    int genomic_sign = genomic_strand == eNa_strand_minus ? -1 : 1;
    TSeqPos exon_bio_start_pos = genomic_sign > 0 ? spliced_exon.GetGenomic_start() : spliced_exon.GetGenomic_end(); 
    exon_loc->SetPacked_int();
    ITERATE(TExonStructure, it, exon_structure) {
        int chunk_length = it->first;
        int chunk_offset = it == exon_structure.begin() ? 0 : (it-1)->second; 

        if(chunk_length == 0) {
            //can happen if we have a gap-only chunk (e.g. arising from truncating alignment to CDS)
            continue;
        }

        TSeqPos bio_start = exon_bio_start_pos + (chunk_offset * genomic_sign);
        TSeqPos bio_stop = bio_start + (chunk_length - 1) * genomic_sign; //-1 because stop is inclusive
 
        CRef<CSeq_interval> chunk(new CSeq_interval);
        chunk->SetId().Assign(genomic_id);
        chunk->SetStrand(genomic_strand);
        chunk->SetFrom(genomic_sign > 0 ? bio_start : bio_stop);
        chunk->SetTo(genomic_sign > 0 ? bio_stop : bio_start);
        exon_loc->SetPacked_int().Set().push_back(chunk);
    }

    return exon_loc;
}


/// Create an exon with the structure consisting of two diags extending inwards from the exon terminals
/// with a single gap of required length in the middle. 
/// This is used for projecting cds-exons consisting entirely of gaps (see ProjectExons)
CRef<CSpliced_exon> CollapseExonStructure(const CSpliced_exon& orig_exon)
{
    CRef<CSpliced_exon> exon(SerialClone(orig_exon));
    TSeqPos query_range = exon->GetProduct_end().GetNucpos() - exon->GetProduct_start().GetNucpos() + 1;
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


// Creating exon-loc for CDS:
//
// Caveat-1:
// A naive way to project CDS would be to take the genomic cds-range and intersect with projected RNA.
// Such approach is clear, but will not work when the genomic cds boundary is in the overlap of the
// exon chunks (in product-ins).
// Instead, we'll truncate the original spliced-seg down to product-CDS and will 
// generate exons-loc the same way as for RNA.
//
// Caveat-2: a CDS on product can itself have discontinuities (e.g. ribosomal slippage), 
// and simply projecting truncated-to-cds alignment will not capture these.
// Instead, we'll take each product-cds chunk, tructate alignment to that, project, and combine the results.
// During the combination of results we'll have to combine sublocs pertaining to same exon
// in the same subloc of the container mix.
//
// Caveat-3: currently Seq-loc-Mapper has a bug, such that truncating an alignment to CDS
// yields wrong result for multi-exon cases (CXX-3724), so to work-around that we'll create a
// single-exon alignment for each exon. Additionally, doing it exon-by-exon makes it easy to
// combine projected result for each exon (from multiple cds sublocs).
//  
// Caveat-4: Remapping may produce gap-only exons, which would ordinarily yield no genomic projection
// counterpart, but in the context of discontinuity-preservation we'll need to calculate genomic
// projection "manually".
CRef<CSeq_loc> ProjectCDSExon(const CSeq_align& spliced_aln,
                              const CSpliced_exon& spliced_exon,
                              const CSeq_loc& product_cds_loc)
{
    CRef<CSeq_align> exon_aln(SerialClone(spliced_aln)); //will create an alignment for each exon separately
    exon_aln->ResetScore();
    exon_aln->ResetExt();

    //Create alignment to represent only the current exon
    exon_aln->SetSegs().SetSpliced().SetExons().clear();
    exon_aln->SetSegs().SetSpliced().SetExons().push_back(CRef<CSpliced_exon>(SerialClone(spliced_exon)));
    CRef<CSeq_loc> query_exon_loc = exon_aln->CreateRowSeq_loc(0);

    CRef<CSeq_loc> exon_loc(new CSeq_loc(CSeq_loc::e_Packed_int));

    for(CSeq_loc_CI ci(product_cds_loc, CSeq_loc_CI::eEmpty_Skip, CSeq_loc_CI::eOrder_Biological); ci; ++ci) {
        CConstRef<CSeq_loc> cds_subloc = ci.GetRangeAsSeq_loc();
        
        if(sequence::eNoOverlap == sequence::Compare(*query_exon_loc, *cds_subloc, NULL)) {
            // exon does not overlap the CDS interval 
            // (i.e. UTR-only, or, in rare case of translational-frameshifts, not specific to this cds-chunk)
            continue;
        }

        //truncate the exon-alignment to the query-cds-subloc
        CRef<CSeq_loc_Mapper> mapper(new CSeq_loc_Mapper(*cds_subloc, *cds_subloc, NULL));
        CRef<CSeq_align> truncated_exon_aln = mapper->Map(*exon_aln);

        if(truncated_exon_aln->GetSegs().GetSpliced().GetExons().size() == 0) {
            // NcbiCerr << "gap-only cds-exon: " << MSerial_AsnText <<spliced_aln;
            // This is a rare case where the exon overlaps the CDS, but truncating the alignment to the CDS
            // produced empty alignment - how can this happen? This is the case where an exon has a product-ins
            // abutting the exon terminal, and the CDS part does not extend past the gap, such that the result of
            // truncation is a gap-only alignment. To deal with this we'll take a chunk of required length
            // starting at genomic exon boundary (i.e. as if the exon structure abutted a diag rather than a gap).
            //
            // We'll do this by ignoring the exon structure, and instead create a 
            // dummy exon consisting of two diags extending from the
            // exon terminals with a gap of necessary length in the middle.
            //
            // Note: The result is the same as if the seq-loc-mapper preserved the gap-only alignment instead of 
            // throwing away the exon, which would result in |product-ins| nucleotides being translated from the
            // genomic exon boundary.
            CRef<CSpliced_exon> collapsed_exon = CollapseExonStructure(*exon_aln->SetSegs().SetSpliced().SetExons().front());   
            exon_aln->SetSegs().SetSpliced().SetExons().front() = collapsed_exon;
            truncated_exon_aln = mapper->Map(*exon_aln);
            if(truncated_exon_aln->GetSegs().GetSpliced().GetExons().size() == 0) {
                continue; //theoretically this shouldn't happen, but we can't proceed otherwise
            }            
        }

        CRef<CSeq_loc> exon_subloc = ProjectExon(
                *truncated_exon_aln->GetSegs().GetSpliced().GetExons().front(),
                spliced_aln.GetSeq_id(1),
                spliced_aln.GetSeqStrand(1));

        AugmentPartialness(*exon_subloc, T53Partialness(cds_subloc->IsPartialStart(eExtreme_Biological),
                                                        cds_subloc->IsPartialStop(eExtreme_Biological)));

        exon_loc->SetPacked_int().Set().insert(exon_loc->SetPacked_int().Set().end(),
                                               exon_subloc->SetPacked_int().Set().begin(),
                                               exon_subloc->SetPacked_int().Set().end());

    }
    return exon_loc;
}


CRef<CSeq_loc> ProjectExons(const CSeq_align& spliced_aln, 
                            CConstRef<CSeq_loc> product_cds_loc, 
                            size_t unaligned_ends_partialness_thr = 0)
{
    CRef<CSeq_loc> exons_loc(new CSeq_loc(CSeq_loc::e_Mix));
    ITERATE(CSpliced_seg::TExons, it, spliced_aln.GetSegs().GetSpliced().GetExons()) {
        const CSpliced_exon& spliced_exon = **it;
       
        CRef<CSeq_loc> exon_loc = product_cds_loc ? 
            ProjectCDSExon(spliced_aln, spliced_exon, *product_cds_loc)
          : ProjectExon(spliced_exon, spliced_aln.GetSeq_id(1), spliced_aln.GetSeqStrand(1));

        AugmentPartialness(*exon_loc, GetExonPartialness(spliced_aln, spliced_exon));
        exons_loc->SetMix().Set().push_back(exon_loc);
    }

    Canonicalize(*exons_loc);

    AugmentPartialness(*exons_loc, 
                       GetTerminalPartialness(spliced_aln, 
                                              product_cds_loc, 
                                              unaligned_ends_partialness_thr));

    return exons_loc;
}


/// Precondition: input loc is discontinuity-preserving RNA loc
/// Postcontition: adjacent packed-ints having the discontinuity between them entirely outside of 
/// cds-range are merged into single interval.
CRef<CSeq_loc> CollapseDiscontinuitiesInUTR(const CSeq_loc& loc, TSeqPos cds_start, TSeqPos cds_stop)
{
    CRef<CSeq_loc> collapsed_loc(new CSeq_loc(CSeq_loc::e_Null));
    if(loc.IsMix()) {
        //each subloc is an exon - recurse on each.
        collapsed_loc->SetMix();
        ITERATE(CSeq_loc::TMix::Tdata, it, loc.GetMix().Get()) {
            CRef<CSeq_loc> collapsed_exon_loc = CollapseDiscontinuitiesInUTR(**it, cds_start, cds_stop);
            collapsed_loc->SetMix().Set().push_back(collapsed_exon_loc);
        }
    } else if(loc.IsPacked_int()) {
        //each subloc is a chunk in an exon - will merge compatible adjacent chunks iff outside of CDS
        collapsed_loc->SetPacked_int();
        ITERATE(CPacked_seqint::Tdata, it, loc.GetPacked_int().Get()) {
            const CSeq_interval& interval = **it;
            
            if(collapsed_loc->GetPacked_int().Get().empty()) {
                collapsed_loc->SetPacked_int().Set().push_back(CRef<CSeq_interval>(SerialClone(interval)));
                continue;
            }
            
            CSeq_interval& last_interval = *collapsed_loc->SetPacked_int().Set().back();
       
            //We can collapse intervals iff the discontinuity (overlap or gap) between them
            //lies outside of the CDS. Equivalently, the count of interval terminals overlapping CDS
            //being at most 1 is necessary and sufficient (i.e. allowing for one of the intervals to be partially in the CDS)
            
            size_t count_terminals_within_cds =
                (last_interval.GetFrom() >= cds_start && last_interval.GetFrom() <= cds_stop ? 1 : 0)
              + (     interval.GetFrom() >= cds_start && last_interval.GetFrom() <= cds_stop ? 1 : 0)
              + (last_interval.GetTo()   >= cds_start && last_interval.GetTo()   <= cds_stop ? 1 : 0)
              + (     interval.GetTo()   >= cds_start &&      interval.GetTo()   <= cds_stop ? 1 : 0);
 
            if(   count_terminals_within_cds <= 1
               && last_interval.GetStrand() == interval.GetStrand()
               && last_interval.GetId().Equals(interval.GetId()))
            {
                TSeqPos union_from = min(interval.GetFrom(), last_interval.GetFrom());
                TSeqPos union_to   = max(interval.GetTo(),   last_interval.GetTo());
                last_interval.SetFrom(union_from);
                last_interval.SetTo(union_to);
            } else {
                collapsed_loc->SetPacked_int().Set().push_back(CRef<CSeq_interval>(SerialClone(interval)));
            }
        }
        
        //even if the original was canonicalized, 
        //we may have collapsed packed-int sublocs such that there's only one remaining,
        //so need to recanonicalize
        Canonicalize(*collapsed_loc);
    } else {
        collapsed_loc->Assign(loc);
    }

    return collapsed_loc;
}


//////////////////////////////////////////////////////////////////////////////////////////////////////////////

CRef<CSeq_loc> CFeatureGenerator::s_ProjectRNA(const CSeq_align& spliced_aln, 
                          CConstRef<CSeq_loc> product_cds_loc,
                          size_t unaligned_ends_partialness_thr)
{
    CRef<CSeq_loc> projected_rna_loc = ProjectExons(spliced_aln,
                                                    CConstRef<CSeq_loc>(NULL),
                                                    unaligned_ends_partialness_thr);

    TSeqPos cds_start(kInvalidSeqPos), cds_stop(kInvalidSeqPos);
    if(product_cds_loc) {
        CRef<CSeq_loc_Mapper> mapper(new CSeq_loc_Mapper(spliced_aln, 1, NULL));
        CRef<CSeq_loc> genomic_cds_range = mapper->Map(*product_cds_loc);
        genomic_cds_range = sequence::Seq_loc_Merge(*genomic_cds_range, CSeq_loc::fMerge_SingleRange, NULL);
        cds_start = genomic_cds_range->GetStart(eExtreme_Positional);
        cds_stop = genomic_cds_range->GetStop(eExtreme_Positional);
    }

    //note, if there's no product-cds-loc, this will collapse discontinuities in every exon
    return CollapseDiscontinuitiesInUTR(*projected_rna_loc, cds_start, cds_stop);
}

CRef<CSeq_loc> CFeatureGenerator::s_ProjectCDS(const CSeq_align& spliced_aln, const CSeq_loc& product_cds_loc)
{
    return ProjectExons(spliced_aln, CConstRef<CSeq_loc>(&product_cds_loc));
}


//////////////////////////////////////////////////////////////////////////////////////////////////////////////

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
