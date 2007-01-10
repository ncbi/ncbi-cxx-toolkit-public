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
#include <objects/seqloc/Seq_point.hpp>
#include <objects/seqalign/seqalign_exception.hpp>
#include <objects/seqloc/Seq_loc.hpp>
#include <objects/seqloc/Seq_interval.hpp>

#include <objtools/alnmgr/alnexception.hpp>
#include <objtools/alnmgr/seqid_comp.hpp>
#include <objtools/alnmgr/aln_seqid.hpp>


BEGIN_NCBI_SCOPE
USING_SCOPE(objects);



/// IAlnSeqId extracting functor
template <class TAlnSeqId>
class CAlnSeqIdsExtract
{
public:
    typedef vector<TAlnSeqIdIRef> TIdVec;

    void operator()(const CSeq_align& seq_align, //< Input aln
                    TIdVec& id_vec) const     //< Output (ids for that aln)
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
                        if (id_vec != next_id_vec) {
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
                            id_vec[row].Reset(new TAlnSeqId(**id_it));
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
                    id_vec[i].Reset(new TAlnSeqId(*ids[i]));
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
                            id.Reset(new TAlnSeqId((*loc_it)->GetEmpty()));
                            break;
                        case CSeq_loc::e_Int:
                            id.Reset(new TAlnSeqId((*loc_it)->GetInt().GetId()));
                            break;
                        case CSeq_loc::e_Pnt:
                            id.Reset(new TAlnSeqId((*loc_it)->GetPnt().GetId()));
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
                        if (min_len == 0  ||  min_len > *len_i) {
                            min_len = *len_i;
                        }
                        if (max_len < *len_i) {
                            max_len = *len_i;
                        }
                    }
                    if (min_len < max_len) {
                        _ASSERT(min_len == max_len / 3  ||
                                min_len - 1 == max_len / 3);
                        for (size_t i=0;  i< seg_lens.size();  ++i) {
                            id_vec[i]->SetBaseWidth(seg_lens[i] == min_len ? 3 : 1);
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
                    id_vec[i].Reset(new TAlnSeqId(*ids[i]));
                }
            }
            break;
        case TSegs::e_Sparse:
            {
//                 const CSparse_seg::TRows& rows = segs.GetSparse().GetRows();
//                 for (size_t row = 0;  row < rows.size();  ++row) {
//                     const CSparse_align& sa = rows[row];
//                     if (row == 0) {
//                         id_vec.resize(segs.GetSparce().GetRows().size() + 1);
//                         id_vec[0].Reset(sa.GetFirst_id());
//                     } else if (*id_vec[0] != sa.GetFirst_id()) {
//                         string err("Inconsistent Seq-ids found in row ");
//                         err += NStr::IntToString(row) + ".";
//                         NCBI_THROW(CAlnException, eInvalidSeqId, err);
//                     }
//                     id_vec[row + 1].Reset(sa.GetSecond_id());
//                 }
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
};


END_NCBI_SCOPE


/*
* ===========================================================================
*
* $Log$
* Revision 1.7  2007/01/10 19:32:32  ucko
* #include <objtools/alnmgr/alnexception.hpp>
*
* Revision 1.6  2007/01/10 18:14:41  todorov
* Vector->Vec
* CSeqalignException->CAlnException
* Fixed a bug with diag ids verification.
*
* Revision 1.5  2007/01/08 16:38:46  todorov
* Fixed a small bug.
*
* Revision 1.4  2006/12/13 14:38:35  todorov
* Adjusted min_len / max_len assertion.
*
* Revision 1.3  2006/12/12 20:53:21  todorov
* Update CAlnSeqIdsExtract per the new IAlnSeqId.
* Deduce the base widths automatically.
*
* Revision 1.2  2006/12/12 20:23:10  ucko
* Replace heterogenous assign() with resize() + copy() for compatibility
* with WorkShop's STL implementation.
*
* Revision 1.1  2006/10/17 19:19:33  todorov
* Initial revision.
*
* ===========================================================================
*/

#endif  // OBJTOOLS_ALNMGR___SEQIDS_EXTRACTOR__HPP
