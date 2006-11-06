#ifndef OBJTOOLS_ALNMGR___ALN_FILTERS__HPP
#define OBJTOOLS_ALNMGR___ALN_FILTERS__HPP
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
*   Filters on Seq-align containers
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

#include <objtools/alnmgr/aln_container.hpp>



/// Implementation includes


BEGIN_NCBI_SCOPE
USING_SCOPE(objects);


class CNonDiagFilter
{
public:
    CQueryAnchoredTest(CScope& scope,
                       const CAlnContainer& in,
                       CAlnContainer& out)
        m_Scope(scope),
        m_AlnContainer(aln_container),
        m_Size(m_AlnContainer.size()),
        m_AlnIdx(0)
    {
        typedef CUnaryMethod<CQueryAnchoredTest, const CSeq_id&, void> TInsertSeqId;
        TInsertSeqId seq_id_inserter(*this, &CQueryAnchoredTest::x_InsertSeqId);

        SFindSeqIds<TInsertSeqId> find_seq_ids(seq_id_inserter);
        
        ITERATE(CAlnContainer, sa_it, aln_container) {
            find_seq_ids(**sa_it);
            ++m_AlnIdx;
        }
    }

    bool operator()() {
        ITERATE(TSeqIdAlnMap, it, m_BitMap) {
            if (it->second.count() == m_Size) {
                return true;
            }
        }
        return false;
    }

private:
    void x_InsertSeqId(const CSeq_id& seq_id) {
        CBioseq_Handle bioseq_handle = m_Scope.GetBioseqHandle(seq_id);
        
        if ( !bioseq_handle ) {
            string err_str = 
                string("Seq-id cannot be resolved: ")
                + seq_id.AsFastaString();
            NCBI_THROW(CSeqalignException, eInvalidAlignment, err_str);
        }
        
        m_BitMap[bioseq_handle][m_AlnIdx] = true;
    }

    typedef map<CBioseq_Handle, bm::bvector<> > TSeqIdAlnMap;
    TSeqIdAlnMap m_BitMap;
    CScope& m_Scope;
    const CAlnContainer& m_AlnContainer;
    const size_t m_Size;
    size_t m_AlnIdx;
};
        


END_NCBI_SCOPE


/*
* ===========================================================================
* $Log$
* Revision 1.1  2006/11/06 20:18:05  todorov
* Initial revision.
*
* ===========================================================================
*/

#endif  // OBJTOOLS_ALNMGR___ALN_FILTERS__HPP
