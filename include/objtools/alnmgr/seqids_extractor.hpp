#ifndef OBJTOOLS_ALNMGR___SEQIDS_EXTRACTOR__HPP
#define OBJTOOLS_ALNMGR___SEQIDS_EXTRACTOR__HPP
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
* Author:  Kamen Todorov, NCBI
*
* File Description:
*   Extract Seq-ids from Seq-align
*
* ===========================================================================
*/


#include <corelib/ncbistd.hpp>
#include <corelib/ncbiobj.hpp>

#include <objects/seqalign/Seq_align_set.hpp>
#include <objects/seqalign/Seq_align.hpp>
#include <objects/seqalign/Dense_diag.hpp>
#include <objects/seqalign/Dense_seg.hpp>
#include <objects/seqalign/Std_seg.hpp>
#include <objects/seqalign/Packed_seg.hpp>
#include <objects/seqalign/Spliced_seg.hpp>
#include <objects/seqalign/Sparse_seg.hpp>
#include <objects/seqalign/Sparse_align.hpp>
#include <objects/seqalign/seqalign_exception.hpp>

#include <objects/seqloc/Seq_point.hpp>
#include <objects/seqloc/Seq_loc.hpp>
#include <objects/seqloc/Seq_interval.hpp>

#include <objtools/alnmgr/alnexception.hpp>
#include <objtools/alnmgr/seqid_comp.hpp>
#include <objtools/alnmgr/aln_seqid.hpp>


BEGIN_NCBI_SCOPE
USING_SCOPE(objects);


template <class TAlnSeqId>
class CAlnSeqIdConverter
{
public:
    TAlnSeqId* operator() (const CSeq_id& id) const {
        return new TAlnSeqId(id);
    }
};


/// IAlnSeqId extracting functor
template <class TAlnSeqId, class TIdConverter = CAlnSeqIdConverter<TAlnSeqId> >
class CAlnSeqIdsExtract
{
public:
    CAlnSeqIdsExtract(void) {}
    CAlnSeqIdsExtract(const TIdConverter& id_conv) : m_IdConv(id_conv) {}

    typedef vector<TAlnSeqIdIRef> TIdVec;

    void operator()(const CSeq_align& seq_align, //< Input aln
                    TIdVec& id_vec) const        //< Output (ids for that aln)
    {
        _ASSERT(id_vec.empty());

        typedef CSeq_align::TDim TDim;
        typedef CSeq_align::TSegs TSegs;

        seq_align.Validate(true);
        
        const TSegs& segs = seq_align.GetSegs();

        switch (segs.Which()) {
        case TSegs::e_Disc:
            {
                bool first_disc = true;
                ITERATE(CSeq_align_set::Tdata, sa_it, segs.GetDisc().Get()) {
                    if (first_disc) {
                        first_disc = false;
                        this->operator()(**sa_it, id_vec);
                    } else {
                        /// Need to make sure ids are identical across all alignments
                        TIdVec next_id_vec;
                        this->operator()(**sa_it, next_id_vec);
                        if ( !IdVecEqual(id_vec, next_id_vec) ) {
                            NCBI_THROW(CAlnException, eInvalidSeqId,
                                       "Inconsistent Seq-ids across the disc alignments.");
                        }
                    }
                }
            }
            break;
        case TSegs::e_Dendiag:
            {
                bool first_diag = true;
                ITERATE(TSegs::TDendiag, diag_it, segs.GetDendiag()) {
                    const CDense_diag::TIds& ids = (*diag_it)->GetIds();
            
                    if (first_diag) {
                        id_vec.resize(ids.size());
                    } else if (id_vec.size() != ids.size()) {
                        NCBI_THROW(CAlnException, eInvalidSeqId,
                                   "Inconsistent Seq-ids.");
                    }
                    size_t row = 0;
                    ITERATE(CDense_diag::TIds, id_it, ids) {
                        if (first_diag) {
                            id_vec[row].Reset(NewAlnSeqId(**id_it));
                        } else if (*id_vec[row] != TAlnSeqId(**id_it)) {
                            NCBI_THROW(CAlnException, eInvalidSeqId,
                                       string("Inconsistent Seq-ids: ") +
                                       id_vec[row]->AsString() + " != " +
                                       TAlnSeqId(**id_it).AsString() + ".");
                        }
                        ++row;
                    }
                    first_diag = false;
                }
            }
            break;
        case TSegs::e_Denseg:
            {
                const CDense_seg::TIds& ids = segs.GetDenseg().GetIds();
                id_vec.resize(ids.size());
                for(size_t i = 0;  i < ids.size();  ++i) {
                    id_vec[i].Reset(NewAlnSeqId(*ids[i]));
                }
            }
            break;
        case TSegs::e_Std: 
            {
                bool first_seg = true;

                typedef CSeq_loc::TRange::position_type TLen;
                typedef vector<TLen> TLenVec;

                ITERATE(TSegs::TStd, std_it, segs.GetStd()) {
                    const CStd_seg& std_seg = **std_it;
                    if (first_seg) {
                        id_vec.resize(std_seg.GetDim());
                    } else if (id_vec.size() != (size_t) std_seg.GetDim()) {
                        NCBI_THROW(CAlnException, eInvalidAlignment,
                                   "The Std-seg dim's need to be consistent.");
                    }
                    if (std_seg.GetLoc().size() != id_vec.size()) {
                        NCBI_THROW(CAlnException, eInvalidAlignment,
                                   "Number of seq-locs inconsistent with dim.");
                    }
                    size_t i = 0;
                    TAlnSeqIdIRef id;

                    // Will record the seg_lens here:
                    TLenVec seg_lens(std_seg.GetDim());

                    ITERATE (CStd_seg::TLoc, loc_it, std_seg.GetLoc()) {
                        switch ((*loc_it)->Which()) {
                        case CSeq_loc::e_Empty:
                            id.Reset(NewAlnSeqId((*loc_it)->GetEmpty()));
                            break;
                        case CSeq_loc::e_Int:
                            id.Reset(NewAlnSeqId((*loc_it)->GetInt().GetId()));
                            break;
                        case CSeq_loc::e_Pnt:
                            id.Reset(NewAlnSeqId((*loc_it)->GetPnt().GetId()));
                            break;
                        default:
                            string err_str =
                                string("Seq-loc of type ") + 
                                (*loc_it)->SelectionName((*loc_it)->Which()) +
                                "is not supported.";
                            NCBI_THROW(CAlnException, eUnsupported, err_str);
                        }

                        // Store the lengths
                        seg_lens[i] = (*loc_it)->GetTotalRange().GetLength();

                        if (first_seg) {
                            id_vec[i].Reset(id);
                        } else if (*id_vec[i] != *id) {
                            string err("Inconsistent Seq-ids found in seg ");
                            err += NStr::IntToString(i) + 
                                ".  Excpected " + id_vec[i]->AsString() +
                                ", encountered " + id->AsString() + ".";
                            NCBI_THROW(CAlnException, eInvalidSeqId, err);
                        }
                        ++i;
                    }

                    // Try to figure out the base_widths
                    // by examining the seg_lens
                    TLen min_len = 0;
                    TLen max_len = 0;
                    ITERATE(TLenVec, len_i, seg_lens) {
                        if (*len_i == 0) continue;
                        if (min_len == 0  ||  min_len > *len_i) {
                            min_len = *len_i;
                        }
                        if (max_len < *len_i) {
                            max_len = *len_i;
                        }
                    }
                    if (min_len < max_len) {
                        bool nuc_prot_diag = (min_len == max_len / 3  ||
                                              min_len - 1 == max_len / 3 ?
                                              true : false);
                        for (size_t i=0;  i< seg_lens.size();  ++i) {
                            id_vec[i]->SetBaseWidth(nuc_prot_diag ? 
                                                    (seg_lens[i] == min_len ? 3 : 1) :
                                                    -1);
                        }
                    }

                    first_seg = false;
                }
                
            }
            break;
        case TSegs::e_Packed:
            {
                const CPacked_seg::TIds& ids = segs.GetPacked().GetIds();
                id_vec.resize(ids.size());
                for(size_t i = 0;  i < ids.size();  ++i) {
                    id_vec[i].Reset(NewAlnSeqId(*ids[i]));
                }
            }
            break;
        case TSegs::e_Sparse:
            {
                const CSparse_seg::TRows& rows = segs.GetSparse().GetRows();
                for (size_t row = 0;  row < rows.size();  ++row) {
                    const CSparse_align& sa = *rows[row];
                    TAlnSeqIdIRef first_id(NewAlnSeqId(sa.GetFirst_id()));
                    if (row == 0) {
                        id_vec.resize(segs.GetSparse().GetRows().size() + 1);
                        id_vec[0].Reset(first_id);
                    } else if (*id_vec[0] != *first_id) {
                        string err("Inconsistent Seq-ids found in row ");
                        err += NStr::IntToString(row) + ".";
                        NCBI_THROW(CAlnException, eInvalidSeqId, err);
                    }
                    id_vec[row + 1].Reset(NewAlnSeqId(sa.GetSecond_id()));
                }
            }
            break;
        case TSegs::e_Spliced:
            {
                const CSpliced_seg& spliced_seg = segs.GetSpliced();
                id_vec.resize(2);

                bool prot = spliced_seg.GetProduct_type() == CSpliced_seg::eProduct_type_protein;

                id_vec[0].Reset(NewAlnSeqId(spliced_seg.GetProduct_id()));
                id_vec[0]->SetBaseWidth(prot ? 3 : 1);

                id_vec[1].Reset(NewAlnSeqId(spliced_seg.GetGenomic_id()));
                id_vec[1]->SetBaseWidth(1);
            }
            break;
        case TSegs::e_not_set:
            NCBI_THROW(CAlnException, eInvalidAlignment,
                       "Seq-align.segs not set.");
        default:
            NCBI_THROW(CAlnException, eUnsupported,
                       "This type of alignment is not supported.");
        }
    
    }

    TAlnSeqId* NewAlnSeqId(const CSeq_id& id) const {
        return m_IdConv(id);
    }

    static bool IdVecEqual(const TIdVec& x, const TIdVec& y) {
        return x.size() == y.size()  &&
            equal(x.begin(), x.end(), y.begin(), SAlnSeqIdRefEqual());
    }

private:
    TIdConverter m_IdConv;
};


/// Typical usage:
typedef CAlnSeqIdsExtract<CAlnSeqId> TIdExtract;



END_NCBI_SCOPE

#endif  // OBJTOOLS_ALNMGR___SEQIDS_EXTRACTOR__HPP

