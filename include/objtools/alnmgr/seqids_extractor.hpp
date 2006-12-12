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

#include <objtools/alnmgr/seqid_comp.hpp>


BEGIN_NCBI_SCOPE
USING_SCOPE(objects);



template <class TSeqIdPtrComp>
class CSeqIdsExtractor
{
public:
    CSeqIdsExtractor(TSeqIdPtrComp&  seq_id_ptr_comp)
        : m_Comp(seq_id_ptr_comp)
    {}

private:
    typedef const CSeq_id* TSeqIdPtr;
    typedef vector<TSeqIdPtr> TSeqIdVector;

public:
    void operator()(const CSeq_align& seq_align, TSeqIdVector& seq_id_vec) const
    {
        _ASSERT(seq_id_vec.empty());

        seq_align.Validate(true);

        typedef CSeq_align::TDim TDim;
        
        typedef CSeq_align::TSegs TSegs;

        const TSegs& segs = seq_align.GetSegs();

        switch (segs.Which()) {
        case TSegs::e_Disc:
            {
                bool first_disc = true;
                ITERATE(CSeq_align_set::Tdata, sa_it, segs.GetDisc().Get()) {
                    if (first_disc) {
                        first_disc = false;
                        this->operator()(**sa_it, seq_id_vec);
                    } else {
                        /// Need to make sure ids are identical across all alignments
                        TSeqIdVector next_seq_id_vec;
                        this->operator()(**sa_it, next_seq_id_vec);
                        if (seq_id_vec != next_seq_id_vec) {
                            NCBI_THROW(CSeqalignException, eInvalidSeqId,
                                       "Inconsistent Seq-ids across the disc alignments.");
                        }
                    }
                }
            }
            break;
        case TSegs::e_Dendiag:
            /// Need to make sure ids are identical across all alignments
            ITERATE(TSegs::TDendiag, diag_it, segs.GetDendiag()) {
                CDense_diag::TIds ids = (*diag_it)->GetIds();

                bool first = true;
                if (first) {
                    seq_id_vec.resize(ids.size());
                } else if (seq_id_vec.size() != ids.size()) {
                    NCBI_THROW(CSeqalignException, eInvalidSeqId,
                               "Inconsistent Seq-ids.");
                }
                size_t i = 0;
                ITERATE(CDense_diag::TIds, id_it, (*diag_it)->GetIds()) {
                    if (first) {
                        seq_id_vec[i] = *id_it;
                    } else if (m_Comp(seq_id_vec[i], *id_it)  ||
                               m_Comp(*id_it, seq_id_vec[i])) {
                        NCBI_THROW(CSeqalignException, eInvalidSeqId,
                                   "Inconsistent Seq-ids.");
                    }
                }
            }
            break;
        case TSegs::e_Denseg:
            seq_id_vec.resize(segs.GetDenseg().GetIds().size());
            copy(segs.GetDenseg().GetIds().begin(), 
                 segs.GetDenseg().GetIds().end(), seq_id_vec.begin());
            return;
        case TSegs::e_Std: 
            {
                bool first_seg = true;

                ITERATE(TSegs::TStd, std_it, segs.GetStd()) {
                    const CStd_seg& std_seg = **std_it;
                    if (first_seg) {
                        seq_id_vec.resize(std_seg.GetDim());
                    } else if (seq_id_vec.size() != (size_t) std_seg.GetDim()) {
                        NCBI_THROW(CSeqalignException, eInvalidAlignment,
                                   "The Std-seg dim's need to be consistens.");
                    }
                    if (std_seg.GetLoc().size() != seq_id_vec.size()) {
                        NCBI_THROW(CSeqalignException, eInvalidAlignment,
                                   "Number of seq-locs inconsistent with dim.");
                    }
                    size_t i = 0;
                    TSeqIdPtr seq_id_ptr;
                    ITERATE (CStd_seg::TLoc, loc_it, std_seg.GetLoc()) {
                        switch ((*loc_it)->Which()) {
                        case CSeq_loc::e_Empty:
                            seq_id_ptr = &(*loc_it)->GetEmpty();
                            break;
                        case CSeq_loc::e_Int:
                            seq_id_ptr = &(*loc_it)->GetInt().GetId();
                            break;
                        case CSeq_loc::e_Pnt:
                            seq_id_ptr = &(*loc_it)->GetPnt().GetId();
                            break;
                        default:
                            string err_str =
                                string("Seq-loc of type ") + 
                                (*loc_it)->SelectionName((*loc_it)->Which()) +
                                "is not supported.";
                            NCBI_THROW(CSeqalignException, eUnsupported, err_str);
                        }
                        if (first_seg) {
                            seq_id_vec[i] = seq_id_ptr;
                        } else if (m_Comp(seq_id_vec[i], seq_id_ptr)  ||
                                   m_Comp(seq_id_ptr, seq_id_vec[i])) {
                            string err("Inconsistent Seq-ids found in seg ");
                            err += NStr::IntToString(i) + 
                                ".  Excpected " + seq_id_vec[i]->AsFastaString() +
                                ", encountered " + seq_id_ptr->AsFastaString() + ".";
                            NCBI_THROW(CSeqalignException, eInvalidSeqId, err);
                        }
                        ++i;
                    }
                    first_seg = false;
                }
                
            }
            break;
        case TSegs::e_Packed:
            seq_id_vec.resize(segs.GetPacked().GetIds().size());
            copy(segs.GetPacked().GetIds().begin(), 
                 segs.GetPacked().GetIds().end(), seq_id_vec.begin());
            break;
        case TSegs::e_not_set:
            NCBI_THROW(CSeqalignException, eInvalidAlignment,
                       "Seq-align.segs not set.");
        default:
            NCBI_THROW(CSeqalignException, eUnsupported,
                       "This type of alignment is not supported.");
        }
    }
    
private:
    TSeqIdPtrComp& m_Comp;
};


END_NCBI_SCOPE


/*
* ===========================================================================
*
* $Log$
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
