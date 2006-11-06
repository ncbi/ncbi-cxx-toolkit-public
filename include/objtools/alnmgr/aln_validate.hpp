#ifndef OBJTOOLS_ALNMGR___ALN_VALIDATE__HPP
#define OBJTOOLS_ALNMGR___ALN_VALIDATE__HPP
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
*   Strict validation for Seq-aligns
*
* ===========================================================================
*/


#include <corelib/ncbistd.hpp>
#include <corelib/ncbiobj.hpp>

#include <objects/seqalign/Seq_align.hpp>
#include <objects/seqalign/Dense_diag.hpp>
#include <objects/seqalign/Dense_seg.hpp>
#include <objects/seqalign/Std_seg.hpp>
#include <objects/seqalign/Packed_seg.hpp>
#include <objects/seqloc/Seq_point.hpp>
#include <objects/seqalign/seqalign_exception.hpp>

#include <objtools/alnmgr/seqid_comp.hpp>
#include <objtools/alnmgr/aln_container.hpp>

#include <util/bitset/ncbi_bitset.hpp>


/// Implementation includes


BEGIN_NCBI_SCOPE
USING_SCOPE(objects);


template <class TSeqIdPtrEqual>
class CAlnValidate
    : public binary_function<CSeq_align, void>
{
public:
    CAlnValidate(TSeqIdPtrEqual&  seq_id_ptr_equal)
        : m_Equal(seq_id_ptr_equal)
    {}

private:
    typedef const CSeq_id* TSeqIdPtr;
    typedef vector<TSeqIdPtr, TSeqIdPtr> TSeqIdVector;

public:
    void operator()(const CSeq_align& seq_align) const
    {
        seq_align.Validate(true);

        typedef CSeq_align::TDim TDim;

        typedef CSeq_align::TSegs TSegs;

        const TSegs& segs = seq_align.GetSegs();

        switch (segs.Which()) {
        case TSegs::e_Disc:
            /// Need to make sure ids are identical across all alignments
            ITERATE(CSeq_align_set::Tdata, sa_it, segs.GetDisc().Get()) {
                this->operator()(**sa_it);
            }
            break;
        case TSegs::e_Dendiag:
            /// Need to make sure ids are identical across all alignments
            ITERATE(TSegs::TDendiag, diag_it, segs.GetDendiag()) {
                ITERATE(CDense_diag::TIds, id_it, (*diag_it)->GetIds()) {
                    m_Yield(*id_it);
                }
            }
            break;
        case TSegs::e_Denseg:
            /// Trivial
            return;
        case TSegs::e_Std: 
            {
                bool first_seg = true;

                TSeqIdVector seq_id_vec;
                
                ITERATE(TSegs::TStd, std_it, segs.GetStd()) {
                    const CStd_seg& std_seg = **std_it;
                    if (first_seg) {
                        dim = std_seg.GetDim();
                        seq_id_vec.resize(dim);
                    } else {
                        if (dim != std_seg.GetDim()) {
                            NCBI_THROW(CSeqalignException, eInvalidAlignment,
                                       "The Std-seg dim's need to be consistens.");
                        }
                    }
                    if (std_seg.GetLoc().size() != dim) {
                        NCBI_THROW(CSeqalignException, eInvalidAlignment,
                                   "Number of seq-locs inconsistent with dim.");
                    }
                    int i = 0;
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
                        } else {
                            if ( !m_Equal(seq_id_vec[i], seq_id_ptr) ) {
                                NCBI_THROW(CSeqalignException, eInvalidSeqId,
                                           "Inconsistent Seq-ids.");
                            }
                        }
                    }
                    first_seg = false;
                }
                
            }
            break;
        case TSegs::e_Packed:
            ITERATE(CPacked_seg::TIds, id_it, segs.GetPacked().GetIds()) {
                m_Yield(*id_it);
            }
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
    TSeqIdPtrEqual& m_Equal;
};


END_NCBI_SCOPE


/*
* ===========================================================================
* $Log$
* Revision 1.1  2006/11/06 20:18:23  todorov
* Initial revision.
*
* ===========================================================================
*/

#endif  // OBJTOOLS_ALNMGR___ALN_VALIDATE__HPP
