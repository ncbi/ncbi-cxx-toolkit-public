#ifndef OBJECTS_ALNMGR___ALNVEC__HPP
#define OBJECTS_ALNMGR___ALNVEC__HPP

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
*   Access to the actual aligned residues
*
*/


#include <objects/alnmgr/alnmap.hpp>
#include <objects/objmgr/seq_vector.hpp>

BEGIN_NCBI_SCOPE

BEGIN_objects_SCOPE // namespace ncbi::objects::


// forward declarations
class CObjectManager;
class CScope;

class CAlnVec : public CAlnMap
{
    typedef CAlnMap Tparent;
    typedef map<TNumrow, CBioseq_Handle> TBioseqHandleCache;
    typedef map<TNumrow, CRef<CSeqVector> > TSeqVectorCache;

public:

    // constructor
    CAlnVec(const CDense_seg& ds);
    CAlnVec(const CDense_seg& ds, TNumrow anchor);
    CAlnVec(const CDense_seg& ds, CScope& scope);
    CAlnVec(const CDense_seg& ds, TNumrow anchor, CScope& scope);

    // destructor
    ~CAlnVec(void);

    CScope& GetScope(void) const;

    string GetSeqString(TNumrow row, TSeqPos from, TSeqPos to)        const;
    string GetSeqString(TNumrow row, TNumseg seg, TNumseg offset = 0) const;
    string GetSeqString(TNumrow row, const CRange<TSeqPos>& range)    const;

    const CBioseq_Handle& GetBioseqHandle(TNumrow row)                  const;
    CSeqVector::TResidue  GetResidue     (TNumrow row, TSeqPos aln_pos) const;

private:
    // Prohibit copy constructor and assignment operator
    CAlnVec(const CAlnVec& value);
    CAlnVec& operator=(const CAlnVec& value);

    CSeqVector& x_GetSeqVector(TNumrow row) const;

    mutable CRef<CObjectManager>    m_ObjMgr;
    mutable CRef<CScope>            m_Scope;
    mutable TBioseqHandleCache      m_BioseqHandlesCache;
    mutable TSeqVectorCache         m_SeqVectorCache;
    
};

///////////////////////////////////////////////////////////
///////////////////// inline methods //////////////////////
///////////////////////////////////////////////////////////

inline
CSeqVector& CAlnVec::x_GetSeqVector(TNumrow row) const
{
    TSeqVectorCache::iterator iter = m_SeqVectorCache.find (row);
    if (iter != m_SeqVectorCache.end()) {
        return *(iter->second);
    } else {
        return *(m_SeqVectorCache[row] =
                 new CSeqVector (GetBioseqHandle (row).GetSeqVector()));
    }
}


inline 
CSeqVector::TResidue CAlnVec::GetResidue(TNumrow row, TSeqPos aln_pos) const
{
    return x_GetSeqVector(row)[GetSeqPosFromAlnPos(aln_pos, row)];
}

///////////////////////////////////////////////////////////
////////////////// end of inline methods //////////////////
///////////////////////////////////////////////////////////

END_objects_SCOPE // namespace ncbi::objects::

END_NCBI_SCOPE

/*
* ===========================================================================
*
* $Log$
* Revision 1.2  2002/08/29 18:40:53  dicuccio
* added caching mechanism for CSeqVector - this greatly improves speed in
* accessing sequence data.
*
* Revision 1.1  2002/08/23 14:43:50  ucko
* Add the new C++ alignment manager to the public tree (thanks, Kamen!)
*
*
* ===========================================================================
*/

#endif // OBJECTS_ALNMGR___ALNVEC__HPP
