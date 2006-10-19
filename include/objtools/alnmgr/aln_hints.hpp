#ifndef OBJTOOLS_ALNMGR___ALN_HINTS__HPP
#define OBJTOOLS_ALNMGR___ALN_HINTS__HPP
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
* Authors:  Kamen Todorov, NCBI
*
* File Description:
*   Seq-align hints
*
* ===========================================================================
*/


#include <corelib/ncbistd.hpp>
#include <corelib/ncbiobj.hpp>

#include <objmgr/scope.hpp>

BEGIN_NCBI_SCOPE
USING_SCOPE(objects);


class CAlnHints : public CObject
{
public:
    typedef const CSeq_id* TSeqIdPtr;
    typedef binary_function<TSeqIdPtr, TSeqIdPtr, bool> TSeqIdComp;
    
    CAlnHints(const TSeqIdComp& seq_id_comp) :
        m_Comp(seq_id_comp)
    {}

    const CBioseq_Handle& GetAnchorHandle() const {
        return m_AnchorHandle;
    }

private:
    CBioseq_Handle m_AnchorHandle;
    TSeqIdComp& m_Comp;
};


END_NCBI_SCOPE


/*
* ===========================================================================
*
* $Log$
* Revision 1.1  2006/10/19 17:21:51  todorov
* Initial revision.
*
* ===========================================================================
*/

#endif  // OBJTOOLS_ALNMGR___ALN_HINTS__HPP
