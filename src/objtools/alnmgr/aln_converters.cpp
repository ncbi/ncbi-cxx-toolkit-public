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
* Authors:  Kamen Todorov, Andrey Yazhuk, NCBI
*
* File Description:
*   Seq-align converters
*
* ===========================================================================
*/


#include <ncbi_pch.hpp>

#include <objects/seqalign/Dense_seg.hpp>
#include <objects/seqalign/Std_seg.hpp>
#include <objects/seqalign/Seq_align_set.hpp>
#include <objects/seqalign/Dense_diag.hpp>
#include <objects/seqalign/Sparse_seg.hpp>
#include <objects/seqalign/Spliced_seg.hpp>
#include <objects/seqalign/Spliced_exon.hpp>
#include <objects/seqalign/Spliced_exon_chunk.hpp>
#include <objects/seqalign/Product_pos.hpp>
#include <objects/seqalign/Prot_pos.hpp>

#include <objects/seqloc/Seq_loc.hpp>
#include <objects/seqloc/Seq_id.hpp>

#include <objtools/alnmgr/aln_converters.hpp>
#include <objtools/alnmgr/alnexception.hpp>
#include <objtools/error_codes.hpp>


#define NCBI_USE_ERRCODE_X   Objtools_Aln_Conv

BEGIN_NCBI_SCOPE
USING_SCOPE(objects);


void
ConvertSeqAlignToPairwiseAln(CPairwiseAln& pairwise_aln,  ///< output
                             const CSeq_align& sa,        ///< input Seq-align
                             CSeq_align::TDim row_1,      ///< which pair of rows
                             CSeq_align::TDim row_2,
                             CAlnUserOptions::EDirection direction) ///< which direction
{
    _ASSERT(row_1 >=0  &&  row_2 >= 0);
    _ASSERT( !sa.IsSetDim()  ||  sa.GetDim() > max(row_1, row_2));

    typedef CSeq_align::TSegs TSegs;
    const TSegs& segs = sa.GetSegs();

    switch(segs.Which())    {
    case CSeq_align::TSegs::e_Dendiag:
        ConvertDendiagToPairwiseAln(pairwise_aln, segs.GetDendiag(),
                                    row_1, row_2, direction);
        break;
    case CSeq_align::TSegs::e_Denseg: {
        ConvertDensegToPairwiseAln(pairwise_aln, segs.GetDenseg(),
                                   row_1, row_2, direction);
        break;
    }
    case CSeq_align::TSegs::e_Std:
        ConvertStdsegToPairwiseAln(pairwise_aln, segs.GetStd(),
                                   row_1, row_2, direction);
        break;
    case CSeq_align::TSegs::e_Packed:
        break;
    case CSeq_align::TSegs::e_Disc:
        ITERATE(CSeq_align_set::Tdata, sa_it, segs.GetDisc().Get()) {
            ConvertSeqAlignToPairwiseAln(pairwise_aln, **sa_it,
                                         row_1, row_2, direction);
        }
        break;
    case CSeq_align::TSegs::e_Spliced:
        ConvertSplicedToPairwiseAln(pairwise_aln, segs.GetSpliced(),
                                    row_1, row_2, direction);
        break;
    case CSeq_align::TSegs::e_Sparse:
        break;
    case CSeq_align::TSegs::e_not_set:
        NCBI_THROW(CAlnException, eInvalidRequest,
                   "Invalid CSeq_align::TSegs type.");
        break;
    }
}


void
ConvertDensegToPairwiseAln(CPairwiseAln& pairwise_aln,  ///< output
                           const CDense_seg& ds,        ///< input Dense-seg
                           CDense_seg::TDim row_1,      ///< which pair of rows
                           CDense_seg::TDim row_2,
                           CAlnUserOptions::EDirection direction) ///< which direction
{
    _ASSERT(row_1 >=0  &&  row_1 < ds.GetDim());
    _ASSERT(row_2 >=0  &&  row_2 < ds.GetDim());

    const CDense_seg::TNumseg& numseg = ds.GetNumseg();
    const CDense_seg::TDim& dim = ds.GetDim();
    const CDense_seg::TStarts& starts = ds.GetStarts();
    const CDense_seg::TLens& lens = ds.GetLens();
    const CDense_seg::TStrands* strands = 
        ds.IsSetStrands() ? &ds.GetStrands() : NULL;

    CDense_seg::TNumseg seg;
    int pos_1, pos_2;
    for (seg = 0, pos_1 = row_1, pos_2 = row_2;
         seg < numseg;
         ++seg, pos_1 += dim, pos_2 += dim) {
        TSignedSeqPos from_1 = starts[pos_1];
        TSignedSeqPos from_2 = starts[pos_2];
        TSeqPos len = lens[seg];

        /// if not a gap, insert it to the collection
        if (from_1 >= 0  &&  from_2 >= 0)  {

            /// determinte the direction
            bool direct = true;
            if (strands) {
                bool minus_1 = (*strands)[pos_1] == eNa_strand_minus;
                bool minus_2 = (*strands)[pos_2] == eNa_strand_minus;
                direct = minus_1 == minus_2;
            }

            if (direction == CAlnUserOptions::eBothDirections  ||
                (direct ?
                 direction == CAlnUserOptions::eDirect :
                 direction == CAlnUserOptions::eReverse)) {

                /// base-width adjustments
                const int& base_width_1 = pairwise_aln.GetFirstBaseWidth();
                const int& base_width_2 = pairwise_aln.GetSecondBaseWidth();
                if (base_width_1 > 1  ||  base_width_2 > 1) {
                    if (base_width_1 > 1) {
                        from_1 *= base_width_1;
                    }
                    if (base_width_2 > 1) {
                        from_2 *= base_width_2;
                    }
                    if (base_width_1 == base_width_2) {
                        len *= base_width_1;
                    }
                }
                
                /// insert the range
                pairwise_aln.insert(CPairwiseAln::TAlnRng(from_1, from_2, len, direct));

            }
        }
    }
}


void
ConvertStdsegToPairwiseAln(CPairwiseAln& pairwise_aln,          ///< output
                           const CSeq_align::TSegs::TStd& stds, ///< input Stds
                           CSeq_align::TDim row_1,              ///< which pair of rows 
                           CDense_seg::TDim row_2,
                           CAlnUserOptions::EDirection direction) ///< which direction
{
    _ASSERT(row_1 >=0  &&  row_2 >= 0);

    ITERATE (CSeq_align::TSegs::TStd, std_it, stds) {

        const CStd_seg::TLoc& loc = (*std_it)->GetLoc();
        
        _ASSERT((CSeq_align::TDim) loc.size() > max(row_1, row_2));

        CSeq_loc::TRange rng_1 = loc[row_1]->GetTotalRange();
        CSeq_loc::TRange rng_2 = loc[row_2]->GetTotalRange();

        TSeqPos len_1 = rng_1.GetLength();
        TSeqPos len_2 = rng_2.GetLength();

        if (len_1 > 0  &&  len_2 > 0) {

            bool direct = 
                loc[row_1]->IsReverseStrand() == loc[row_2]->IsReverseStrand();

            if (direction == CAlnUserOptions::eBothDirections  ||
                (direct ?
                 direction == CAlnUserOptions::eDirect :
                 direction == CAlnUserOptions::eReverse)) {

                const int& base_width_1 = pairwise_aln.GetFirstBaseWidth();
                const int& base_width_2 = pairwise_aln.GetSecondBaseWidth();

                CPairwiseAln::TAlnRng aln_rng;
                aln_rng.SetDirect(direct);
                if (base_width_1 == base_width_2) {
                    _ASSERT(len_1 == len_2);
                    if (base_width_1 == 1) {
                        aln_rng.SetFirstFrom(rng_1.GetFrom());
                        aln_rng.SetSecondFrom(rng_2.GetFrom());
                    } else {
                        aln_rng.SetFirstFrom(rng_1.GetFrom() * base_width_1);
                        aln_rng.SetSecondFrom(rng_2.GetFrom() * base_width_2);
                    }
                    aln_rng.SetLength(len_1 * base_width_1);
                    pairwise_aln.insert(aln_rng);
                } else if (base_width_1 == 1) {
                    _ASSERT(base_width_2 == 3);
                    aln_rng.SetFirstFrom(rng_1.GetFrom());
                    aln_rng.SetSecondFrom(rng_2.GetFrom() * base_width_2);
                    if (len_1 / base_width_2 < len_2) {
                        _ASSERT(len_1 / base_width_2 == len_2 - 1);
                        TSeqPos remainder = len_1 % base_width_2;
                        aln_rng.SetLength(len_1 - remainder);
                        pairwise_aln.insert(aln_rng);
                        pairwise_aln.insert
                            (CPairwiseAln::TAlnRng
                             (aln_rng.GetFirstToOpen(),
                              aln_rng.GetSecondToOpen(),
                              remainder,
                              direct));
                    } else {
                        aln_rng.SetLength(len_1);
                        pairwise_aln.insert(aln_rng);
                    }
                } else if (base_width_2 == 1) {
                    _ASSERT(base_width_1 == 3);
                    aln_rng.SetFirstFrom(rng_1.GetFrom() * base_width_1);
                    aln_rng.SetSecondFrom(rng_2.GetFrom());
                    if (len_2 / base_width_1 < len_1) {
                        _ASSERT(len_2 / base_width_1 == len_1 - 1);
                        TSeqPos remainder = len_2 % base_width_2;
                        aln_rng.SetLength(len_2 - remainder);
                        pairwise_aln.insert(aln_rng);
                        pairwise_aln.insert
                            (CPairwiseAln::TAlnRng
                             (aln_rng.GetFirstToOpen(),
                              aln_rng.GetSecondToOpen(),
                              remainder,
                              direct));
                    } else {
                        aln_rng.SetLength(len_2);
                        pairwise_aln.insert(aln_rng);
                    }
                } else {
                    _ASSERT(len_1 * base_width_1 == len_2 * base_width_2);
                    aln_rng.SetLength(len_1 * base_width_1);
                    pairwise_aln.insert(aln_rng);
                }
            }
        }
    }
}



void
ConvertDendiagToPairwiseAln(CPairwiseAln& pairwise_aln,                  ///< output
                            const CSeq_align::TSegs::TDendiag& dendiags, ///< input Dendiags
                            CSeq_align::TDim row_1,                      ///< which pair of rows 
                            CDense_seg::TDim row_2,
                            CAlnUserOptions::EDirection direction) ///< which direction
{
    _ASSERT(row_1 >=0  &&  row_2 >= 0);

    ITERATE (CSeq_align::TSegs::TDendiag, dendiag_it, dendiags) {

        const CDense_diag& dd = **dendiag_it;

        _ASSERT(max(row_1, row_2) < dd.GetDim());

        TSeqPos from_1 = dd.GetStarts()[row_1];
        TSeqPos from_2 = dd.GetStarts()[row_2];
        TSeqPos len = dd.GetLen();

        /// determinte the strands
        bool direct = true;
        if (dd.IsSetStrands()) {
            bool minus_1 = dd.GetStrands()[row_1] == eNa_strand_minus;
            bool minus_2 = dd.GetStrands()[row_2] == eNa_strand_minus;
            direct = minus_1 == minus_2;
        }

        if (direction == CAlnUserOptions::eBothDirections  ||
            (direct ?
             direction == CAlnUserOptions::eDirect :
             direction == CAlnUserOptions::eReverse)) {

            /// base-width adjustments
            const int& base_width_1 = pairwise_aln.GetFirstBaseWidth();
            const int& base_width_2 = pairwise_aln.GetSecondBaseWidth();
            if (base_width_1 > 1  ||  base_width_2 > 1) {
                if (base_width_1 > 1) {
                    from_1 *= base_width_1;
                }
                if (base_width_2 > 1) {
                    from_2 *= base_width_2;
                }
                if (base_width_1 == base_width_2) {
                    len *= base_width_1;
                }
            }

            /// insert the range
            pairwise_aln.insert(CPairwiseAln::TAlnRng(from_1, from_2, len, direct));

        }
    }
}


void
ConvertSparseToPairwiseAln(CPairwiseAln& pairwise_aln,    ///< output
                           const CSparse_seg& sparse_seg, ///< input Sparse-seg
                           CSeq_align::TDim row_1,        ///< which pair of rows 
                           CDense_seg::TDim row_2,
                           CAlnUserOptions::EDirection direction) ///< which direction
{
    _ASSERT(row_1 == 0);
    _ASSERT(row_2 > 0  &&  (size_t)row_2 < sparse_seg.GetRows().size() + 1);

//     const CSparse_align& sa = sparse_seg.GetRows()[row_2 - 1];

//     const CSparse_align::TFirst_starts& starts_1 = sa.GetFirst_starts();
//     const CSparse_align::TSecond_starts& starts_2 = sa.GetSecond_starts();
//     const CSparse_align::TLens& lens = sa.GetLens();
//     const CSparse_align::TSecond_strands* strands =
//         sa.IsSetSecond_strands() ? &sa.GetSecond_strands() : 0;

//     CSparse_align::TNumseg seg;
//     for (seg = 0;  seg < sa.GetNumseg();  seg++) {
//         pairwise_aln.insert
//             (CPairwiseAln::TAlnRng(starts_1[seg],
//                                    starts_2[seg],
//                                    lens[seg],
//                                    strands ?
//                                    (*strands)[seg] != eNa_strand_minus :
//                                    true));
//     }
}


void
ConvertSplicedToPairwiseAln(CPairwiseAln& pairwise_aln,      ///< output
                            const CSpliced_seg& spliced_seg, ///< input Spliced-seg
                            CSeq_align::TDim row_1,          ///< which pair of rows 
                            CDense_seg::TDim row_2,
                            CAlnUserOptions::EDirection direction) ///< which direction
{
    _ASSERT(row_1 == 0  ||  row_1 == 1  &&  row_2 == 0  ||  row_2 == 1);

    bool prot = spliced_seg.GetProduct_type() == CSpliced_seg::eProduct_type_protein;

    ITERATE (CSpliced_seg::TExons, exon_it, spliced_seg.GetExons()) {

        const CSpliced_exon& exon = **exon_it;
            
        /// Determine strands
        if (spliced_seg.CanGetProduct_strand()  &&  exon.CanGetProduct_strand()  &&
            spliced_seg.GetProduct_strand() != exon.GetProduct_strand()) {
            NCBI_THROW(CSeqalignException, eInvalidAlignment,
                       "Product strands are not consistent.");
        }
        bool product_plus = true;
        if (exon.CanGetProduct_strand()) {
            product_plus = exon.GetProduct_strand() != eNa_strand_minus;
        } else if (spliced_seg.CanGetProduct_strand()) {
            product_plus = spliced_seg.GetProduct_strand() != eNa_strand_minus;
        }
        _ASSERT(prot ? product_plus : true);

        if (spliced_seg.CanGetGenomic_strand()  &&  exon.CanGetGenomic_strand()  &&
            spliced_seg.GetGenomic_strand() != exon.GetGenomic_strand()) {
            NCBI_THROW(CSeqalignException, eInvalidAlignment,
                       "Genomic strands are not consistent.");
        }
        bool genomic_plus = true;
        if (exon.CanGetGenomic_strand()) {
            genomic_plus = exon.GetGenomic_strand() != eNa_strand_minus;
        } else if (spliced_seg.CanGetGenomic_strand()) {
            genomic_plus = spliced_seg.GetGenomic_strand() != eNa_strand_minus;
        }
        bool direct = product_plus  ==  genomic_plus;
    

        /// Determine positions
        TSeqPos product_pos =
            (prot ?
             exon.GetProduct_start().GetProtpos().GetAmin() * 3 + exon.GetProduct_start().GetProtpos().GetFrame() - 1 :
             (product_plus ?
              exon.GetProduct_start().GetNucpos() :
              exon.GetProduct_end().GetNucpos()));
        
        TSeqPos genomic_pos = (genomic_plus ? 
                               exon.GetGenomic_start() :
                               exon.GetGenomic_end());
            

        /// Iterate trhough exon chunks
        ITERATE (CSpliced_exon::TParts, chunk_it, exon.GetParts()) {
            const CSpliced_exon_chunk& chunk = **chunk_it;
                
            TSeqPos product_len = 0;
            TSeqPos genomic_len = 0;
            
            switch (chunk.Which()) {
            case CSpliced_exon_chunk::e_Match: 
                product_len = genomic_len = chunk.GetMatch();
                break;
            case CSpliced_exon_chunk::e_Diag: 
                product_len = genomic_len = chunk.GetDiag();
                break;
            case CSpliced_exon_chunk::e_Mismatch:
                product_len = genomic_len = chunk.GetMismatch();
                break;
            case CSpliced_exon_chunk::e_Product_ins:
                product_len = chunk.GetProduct_ins();
                break;
            case CSpliced_exon_chunk::e_Genomic_ins:
                genomic_len = chunk.GetGenomic_ins();
                break;
            default:
                break;
            }
            if (!chunk.IsMismatch()) {
                if (row_1 == 0  &&  row_2 == 0) {
                    if (product_len != 0) {
                        /// insert the range
                        pairwise_aln.insert
                            (CPairwiseAln::TAlnRng
                             (product_plus ? product_pos : product_pos - product_len + 1,
                              product_plus ? product_pos : product_pos - product_len + 1,
                              product_len,
                              true));
                    }
                } else if (row_1 == 1  &&  row_2 == 1) {
                    if (genomic_len != 0) {
                        /// insert the range
                        pairwise_aln.insert
                            (CPairwiseAln::TAlnRng
                             (genomic_plus ? genomic_pos : genomic_pos - genomic_len + 1,
                              genomic_plus ? genomic_pos : genomic_pos - genomic_len + 1,
                              genomic_len,
                              true));
                    }
                } else {
                    _ASSERT(row_1 != row_2);
                    if (product_len != 0  &&  product_len == genomic_len  &&
                        (direction == CAlnUserOptions::eBothDirections  ||
                         (direct ?
                          direction == CAlnUserOptions::eDirect :
                          direction == CAlnUserOptions::eReverse))) {
                        /// insert the range
                        pairwise_aln.insert
                            (CPairwiseAln::TAlnRng
                             (product_plus ? product_pos : product_pos - product_len + 1,
                              genomic_plus ? genomic_pos : genomic_pos - genomic_len + 1,
                              genomic_len,
                              direct));
                    }
                }
            }
            if (product_plus) {
                product_pos += product_len;
            } else {
                product_pos -= product_len;
            }
            if (genomic_plus) {
                genomic_pos += genomic_len;
            } else {
                genomic_pos -= genomic_len;
            }
        }
    }
}


void
ConvertSeqLocsToPairwiseAln(CPairwiseAln& aln,
                            const objects::CSeq_loc& loc_1,
                            const objects::CSeq_loc& loc_2,
                            CAlnUserOptions::EDirection direction)
{
    // Make sure each seq-loc contains just one seq-id
    _ASSERT(loc_1.GetId());
    _ASSERT(loc_2.GetId());

    // Rough check if strands are the same (may be false-positive if
    // there are multiple strands).
    bool direct = 
        loc_1.IsReverseStrand() == loc_2.IsReverseStrand();

    if (direction != CAlnUserOptions::eBothDirections  &&
        (direct ?
            direction != CAlnUserOptions::eDirect :
            direction != CAlnUserOptions::eReverse)) {
        return;
    }

    TSeqPos wid1 = aln.GetFirstBaseWidth();
    if ( !wid1 ) {
        wid1 = 1;
    }
    TSeqPos wid2 = aln.GetSecondBaseWidth();
    if ( !wid2 ) {
        wid2 = 1;
    }

    CSeq_loc_CI it1(loc_1);
    CSeq_loc_CI it2(loc_2);
    TSeqPos lshift1 = 0;
    TSeqPos lshift2 = 0;
    TSeqPos rshift1 = 0;
    TSeqPos rshift2 = 0;
    while (it1  &&  it2) {
        if (it1.IsEmpty()) {
            ++it1;
            continue;
        }
        if (it2.IsEmpty()) {
            ++it2;
            continue;
        }
        bool rev1 = IsReverse(it1.GetStrand());
        bool rev2 = IsReverse(it2.GetStrand());
        TSeqPos len1 = (it1.GetRange().GetLength() - lshift1 - rshift1)*wid1;
        TSeqPos len2 = (it2.GetRange().GetLength() - lshift2 - rshift2)*wid2;
        TSeqPos len = len1 > len2 ? len2 : len1;
        TSeqPos start1 = (it1.GetRange().GetFrom() + lshift1)*wid1;
        if ( rev1 ) {
            start1 += len1 - len;
        }
        TSeqPos start2 = (it2.GetRange().GetFrom() + lshift2)*wid2;
        if ( rev2 ) {
            start2 += len2 - len;
        }
        aln.insert(CPairwiseAln::TAlnRng(start1, start2, len, rev1 == rev2));
        if ( rev1 ) {
            rshift1 += len/wid1;
        }
        else {
            lshift1 += len/wid1;
        }
        if ( rev2 ) {
            rshift2 += len/wid2;
        }
        else {
            lshift2 += len/wid2;
        }
        if (len1 == len) {
            ++it1;
            lshift1 = rshift1 = 0;
        }
        if (len2 == len) {
            ++it2;
            lshift2 = rshift2 = 0;
        }
    }
}


typedef map<CSeq_id_Handle, CRef<CPairwiseAln> > TAlnMap;
typedef map<CSeq_id_Handle, CSeq_id_Handle> TSynonymsMap;

CSeq_id_Handle s_GetBestSynonym(const CSeq_id_Handle& idh,
                                TSynonymsMap& syn_map,
                                const CSeq_loc_Mapper_Base& mapper)
{
    TSynonymsMap::const_iterator best_it = syn_map.find(idh);
    if (best_it != syn_map.end()) {
        return best_it->second;
    }
    // Add all synonyms for the new id handle
    CSeq_loc_Mapper_Base::TSynonyms syn_set;
    mapper.CollectSynonyms(idh, syn_set);
    CSeq_id_Handle best_id = idh;
    int best_score = idh.GetSeqId()->BestRankScore();
    ITERATE(CSeq_loc_Mapper_Base::TSynonyms, it, syn_set) {
        int score = it->GetSeqId()->BestRankScore();
        if (score < best_score) {
            best_id = *it;
            best_score = score;
        }
    }
    ITERATE(CSeq_loc_Mapper_Base::TSynonyms, it, syn_set) {
        syn_map[*it] = best_id;
    }
    return best_id;
}


void SeqLocMapperToPairwiseAligns(const objects::CSeq_loc_Mapper_Base& mapper,
                                  TPairwiseAlnList&                    aligns)
{
    aligns.clear();
    TSynonymsMap synonyms;

    const CMappingRanges& mappings = mapper.GetMappingRanges();
    ITERATE(CMappingRanges::TIdMap, id_it, mappings.GetIdMap()) {
        CSeq_id_Handle src_idh =
            s_GetBestSynonym(id_it->first, synonyms, mapper);
        if (src_idh != id_it->first) {
            continue; // skip synonyms
        }
        TAlnSeqIdIRef src_id(Ref(new CAlnSeqId(*src_idh.GetSeqId())));
        src_id->SetBaseWidth(mapper.GetWidthById(src_idh));
        TAlnMap aln_map;
        ITERATE(CMappingRanges::TRangeMap, rg_it, id_it->second) {
            const CMappingRange& mrg = *rg_it->second;
            CSeq_id_Handle dst_idh =
                s_GetBestSynonym(mrg.GetDstIdHandle(), synonyms, mapper);
            if (dst_idh == src_idh) {
                continue; // skip self-mappings
            }
            TAlnMap::iterator aln_it = aln_map.find(dst_idh);
            CRef<CPairwiseAln> aln;
            if (aln_it == aln_map.end()) {
                TAlnSeqIdIRef dst_id(Ref(new CAlnSeqId(*dst_idh.GetSeqId())));
                dst_id->SetBaseWidth(mapper.GetWidthById(dst_idh));
                aln = new CPairwiseAln(src_id, dst_id);
                aln_map[dst_idh] = aln;
                aligns.push_back(aln);
            }
            else {
                aln = aln_it->second;
            }
            aln->insert(CPairwiseAln::TAlnRng(mrg.GetSrc_from(),
                mrg.GetDst_from(), mrg.GetLength(), mrg.GetReverse()));
        }
    }
}


// #include <objtools/alnmgr/pairwise_aln.hpp>

// #include <objects/seqalign/Sparse_align.hpp>
// #include <objects/seqalign/Seq_align.hpp>
// #include <objects/seqloc/Seq_id.hpp>

// #include <objmgr/bioseq_handle.hpp>


// BEGIN_NCBI_SCOPE
// USING_SCOPE(ncbi::objects);


// void 
// CAlnToPairwiseAlnConverter::Convert(const CSeq_align& input_seq_align,
//                                     CPairwiseAln&     output_pairwise_align)
// {
//     typedef CSeq_align::TSegs TSegs;
//     const TSegs& segs = input_seq_align.GetSegs();

//     switch(segs.Which())    {
//     case CSeq_align::TSegs::e_Dendiag:
//         break;
//     case CSeq_align::TSegs::e_Denseg: {
//         CDense_seg::TDim row;
//         CDense_seg::TNumseg sgmt;

//         /// Determine anchor row
//         for (row = 0; row < ds.GetDim(); ++row)   {
//             if ( !(m_Comp(ds.GetIds()[row], m_Hints.GetAnchorId()) ||
//                    m_Comp(m_Hints.GetAnchorId(), ds.GetIds()[row])) ) {
//                 anchor_row = row;
//             }
//         }
         
//         for (row = 0; row < ds.GetDim();  ++row) {
//             if (row != anchor_row) {
//                 CConstRef<CPairwiseAln> pairwise_aln
//                     (new CPairwiseAln(sa, anchor_row, row));
//                 output_pairwise_align.m_PairwiseAlns.push_back(pairwise_aln);
//             }
//         }
//         break;
//     }
//     case CSeq_align::TSegs::e_Std:
//         x_BuildFromStdseg();
//         break;
//     case CSeq_align::TSegs::e_Packed:
//         break;
//     case CSeq_align::TSegs::e_Disc:
//         ITERATE(CSeq_align_set::Tdata, sa_it, segs.GetDisc().Get()) {
//             ConvertCPairwiseAln(**sa_it, row_1, row_2);
//         }
//         break;
// //     case CSeq_align::TSegs::e_Spliced:
// //     case CSeq_align::TSegs::e_Sparse:
//     case CSeq_align::TSegs::e_not_set:
//         break;
//     }

//     case TSegs::e_Disc:
//         {
//             bool first_disc = true;
//             ITERATE(CSeq_align_set::Tdata, sa_it, segs.GetDisc().Get()) {
//                 if (first_disc) {
//                     first_disc = false;
//                     this->operator()(**sa_it, seq_id_vec);
//                 } else {
//                     /// Need to make sure ids are identical across all alignments
//                     TSeqIdVector next_seq_id_vec;
//                     this->operator()(**sa_it, next_seq_id_vec);
//                     if (seq_id_vec != next_seq_id_vec) {
//                         NCBI_THROW(CSeqalignException, eInvalidSeqId,
//                                    "Inconsistent Seq-ids across the disc alignments.");
//                     }
//                 }
//             }
//         }
//         break;
//     case TSegs::e_Denseg:
//         x_Convert(segs.GetDenseg());
//         break;
//     case TSegs::e_Std:
//         ITERATE (TSegs::TStd, std_i, segs.GetStd()) {
//             x_Convert(*std_i);
//         }
//         break;
//     default:
//     }
// }


// void 
// CAlnToPairwiseAlnConverter::x_Convert(const CDense_seg& ds)
// {
// }


// void
// CAlnToPairwiseAlnConverter::x_Convert(const CSparse_align& sparse_align)
// {
// }


// void
// CAlnToPairwiseAlnConverter::x_Convert(const CSparse_seg& sparse_seg)
// {
// }



// // Conversion function CSparse_align -> SAlignedSeq
// SAlignedSeq* CreateAlnRow(const CSparse_align& align, bool master_first)
// {
//     auto_ptr<SAlignedSeq> aln_seq(new SAlignedSeq());
//     aln_seq->m_SeqId.Reset(master_first ? &align.GetSecond_id() 
//                                         : &align.GetFirst_id());   
//     SAlignedSeq::TSignedRange& range = aln_seq->m_SecondRange;

//     // get references to the containers inside CSparse_align
//     const CSparse_align::TFirst_starts& starts_1 = align.GetFirst_starts();
//     const CSparse_align::TSecond_starts& starts_2 = align.GetSecond_starts();
//     const CSparse_align::TLens& lens = align.GetLens();
//     const CSparse_align::TSecond_strands* strands =
//         align.IsSetSecond_strands() ? &align.GetSecond_strands() : 0;

//     // create a new Aln Collection
//     SAlignedSeq::TAlnColl* coll = new SAlignedSeq::TAlnColl();
//     range.SetFrom(0).SetLength(0);
//     SAlignedSeq::TPos aln_from = -1, from = -1;

//     // iterate on Sparse-seg elements
//     typedef CSparse_align::TNumseg TNumseg;
//     for( TNumseg i = 0;  i < align.GetNumseg(); i++  )  {
//         aln_from = master_first ? starts_1[i] : starts_2[i];
//         from = master_first ? starts_2[i] : starts_1[i];
//         SAlignedSeq::TPos len = lens[i];
//         bool dir = strands ? ((*strands)[i] == eNa_strand_plus) : true;

//         // update range
//         if(coll->empty())    {
//             range.SetFrom(aln_from);
//             range.SetLength(len);
//         } else {
//             range.SetFrom(min(range.GetFrom(), aln_from));
//             range.SetToOpen(max(range.GetToOpen(), aln_from + len));
//         }

//         coll->insert(SAlignedSeq::TAlignRange(aln_from, from, len, dir));
//     }
//     aln_seq->m_AlnColl = coll;

//     int dir = (coll->GetFlags() & SAlignedSeq::TAlignColl::fMixedDir);
//     if(dir == SAlignedSeq::TAlignColl::fMixedDir)    { 
//         // incorrect - do not return anything
//         return NULL;
//     } else if(dir == SAlignedSeq::TAlignColl::fReversed) {
//         aln_seq->m_NegativeStrand = true;
//     }
//     return aln_seq.release();        
// }

// /// Converter
// bool ConvertToPairwise(const vector< CConstRef<objects::CSeq_align> >& aligns, 
//                        const CSeq_id& master_id,
//                        vector<SAlignedSeq*>& aln_seqs)
// {
//     CStopWatch timer;
//     timer.Start();

//     bool ok = false;
//     for( size_t i = 0;  i < aligns.size();  i++ )   {
//         const CSeq_align& align = *aligns[i];
//         bool res = ConvertToPairwise(align, master_id, aln_seqs);
//         ok |= res;
//     }
//     LOG_POST_X(1, "ConvertToPairwise( vector of CSeq_align) " << 1000 * timer.Elapsed() << " ms");
//     return ok;
// }


// /// Converter CSparse_seg -> SAlignedSeq-s
// bool ConvertToPairwise(const CSparse_seg& sparse_seg, vector<SAlignedSeq*>& aln_seqs)
// {
//     CConstRef<objects::CSeq_id> master_id(&sparse_seg.GetMaster_id());

//     typedef CSparse_seg::TRows  TRows;
//     const TRows& rows = sparse_seg.GetRows();
//     TRows::const_iterator it = rows.begin();

//     // convert pairwise alignment to TAlignColl objects
//     for(  ;  it != rows.end();  ++it    )   {
//         const CSparse_align& align = **it;

//         int master_index = -1;
//         if(master_id->Compare(align.GetFirst_id()) == CSeq_id::e_YES) {
//             master_index = 0;
//         } else if(master_id->Compare(align.GetSecond_id()) == CSeq_id::e_YES) {
//             master_index = 1;
//         }

//         if(master_index != -1)  {   // create an alignment row from this CSparse_align
//             SAlignedSeq* aln_seq = CreateAlignRow(align, master_index == 0);
//             if(aln_seq) {
//                 aln_seqs.push_back(aln_seq);
//             }
//         } else {
//             LOG_POST_X(2, Error << "CreateAlnRow() - a CSparse_align is"
//                           << "invalid, neither of its CSeq_ids match master id");
//         }
//     }
//     return true; // handle errors
// }


// bool ConvertToPairwise(const CStd_seg& std_seg,
//                        const CSeq_id& master_id,
//                        vector<SAlignedSeq*> aln_segs)
// {
//     ITERATE (CStd_seg::TLoc, loc_it, GetLoc()) {
//         if (loc_it


// /// Builder function
// CSparseAlignment* BuildSparseAlignment(const CSeq_id& master_id,
//                                        vector<SAlignedSeq*>& aln_seqs,
//                                        objects::CScope& scope)
// {
//     if(! aln_seqs.empty()) {
//         CSparseAlignment* aln = new CSparseAlignment();
//         aln->Init(master_id, aln_seqs, scope);
//         return aln;
//     }
//     return NULL;
// }


// /// Builder function
// CAlnVec* BuildDenseAlignment(const CSeq_id& master_id,
//                              vector<SAlignedSeq*>& aln_seqs,
//                              objects::CScope& scope)
// {
//     if(! aln_seqs.empty()) {
//     }
//     return NULL;
// }


// /// Creates Align Collection from a CSparse_seg
// void GetAlignColl(const CSparse_align& sparse_align,
//                   const CSeq_id& master_id, 
//                   SAlignTools::TAlignColl& coll)
// {
//     coll.clear();

//     int index = -1;
//     if(master_id.Compare(sparse_align.GetFirst_id()) == CSeq_id::e_YES) {
//         index = 0;
//     } else if(master_id.Compare(sparse_align.GetSecond_id()) == CSeq_id::e_YES) {
//         index = 1;
//     }
//     if(index != -1) {
//         bool first = (index == 0);
//         const CSparse_align::TFirst_starts& starts_1 = sparse_align.GetFirst_starts();
//         const CSparse_align::TFirst_starts& starts_2 = sparse_align.GetSecond_starts();
//         const CSparse_align::TLens& lens = sparse_align.GetLens();
//         const CSparse_align::TSecond_strands* strands =
//             sparse_align.IsSetSecond_strands() ? &sparse_align.GetSecond_strands() : 0;

//         typedef CSparse_align::TNumseg TNumseg;
//         TNumseg n_seg = sparse_align.GetNumseg();
//         for( TNumseg i = 0;  i < n_seg;  i++ )  {
//             int from_1 = first ? starts_1[i] : starts_2[i];
//             int from_2 = first ? starts_2[i] : starts_1[i];
//             int len = lens[i];
//             bool direct = strands  &&  ((*strands)[i] == eNa_strand_minus);

//             coll.insert(SAlignTools::TAlignRange(from_1, from_2, len, direct));
//         }
//     }
// }


// /// Reverse Converter
// /// Converts Align Collection into a CSparse_align
// CRef<CSparse_align> CreateSparseAlign(const CSeq_id& id_1, 
//                                       const CSeq_id& id_2,
//                                       const SAlignTools::TAlignColl& coll)
// {
//     CRef<CSparse_align> align(new CSparse_align());

//     CRef<CSeq_id> rid_1(new CSeq_id());
//     rid_1->Assign(id_1);
//     align->SetFirst_id(*rid_1);

//     CRef<CSeq_id> rid_2(new CSeq_id());
//     rid_2->Assign(id_2);
//     align->SetSecond_id(*rid_2);

//     // initilize containers
//     typedef CSparse_align::TNumseg TNumseg;
//     TNumseg n_seg = coll.size();
//     align->SetNumseg(n_seg);

//     CSparse_align::TFirst_starts& starts_1 = align->SetFirst_starts();
//     starts_1.resize(n_seg);
//     CSparse_align::TFirst_starts& starts_2 = align->SetSecond_starts();
//     starts_2.resize(n_seg);
//     CSparse_align::TLens& lens = align->SetLens();
//     lens.resize(n_seg);

//     CSparse_align::TSecond_strands* strands = NULL;
//     if(coll.GetFlags()  &  SAlignTools::TAlignColl::fReversed) {
//         // there are reversed segments in the collection - need to fill "Strands"
//         strands = &align->SetSecond_strands();
//         strands->resize(n_seg);
//     }

//     // move data to the containers
//     TNumseg i = 0;
//     ITERATE(SAlignTools::TAlignColl, it, coll)   {
//         const SAlignTools::TAlignRange& r = *it;

//         starts_1[i] = r.GetFirstFrom();
//         starts_2[i] = r.GetSecondFrom();
//         lens[i] = r.GetLength();
//         if(strands)  {
//             (*strands)[i] = r.IsDirect() ? eNa_strand_plus : eNa_strand_minus;
//         }
//         i++;
//     }

//     return align;
// }


END_NCBI_SCOPE
