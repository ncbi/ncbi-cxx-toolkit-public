#ifndef OBJECTS_ALNMGR___ALNMIX__HPP
#define OBJECTS_ALNMGR___ALNMIX__HPP

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
*   Alignment merger
*
*/

#include <objects/alnmgr/alnvec.hpp>
#include <objects/seqalign/Seq_align.hpp>

BEGIN_NCBI_SCOPE

BEGIN_objects_SCOPE // namespace ncbi::objects::


// forward declarations
class CScope;
class CObjectManager;

class CAlnMix
{
public:

    typedef list< CRef< CSeq_align > > TAlns;
    typedef list< CConstRef< CSeq_align > > TConstAlns;
    

    // constructor
    CAlnMix(void);
    CAlnMix(CScope& scope);
    CAlnMix(CConstRef<CSeq_align> aln);
    CAlnMix(CConstRef<CSeq_align> aln, CScope& scope);
    CAlnMix(const TAlns& alns);
    CAlnMix(const TAlns& alns, CScope& scope);

    // destructor
    ~CAlnMix(void);

    void Add(const TAlns& sa_lst);
    void Add(CConstRef<CSeq_align> sa);

    enum EMergeFlags {
        fGen2EST              = 0x01, // otherwise Nucl2Nucl
        fTruncateOverlaps     = 0x02, // otherwise put on separate rows
        fNegativeStrand       = 0x04
    };
    typedef int TMergeFlags; // binary OR of EMergeFlags
    void Merge(TMergeFlags flags = 0);

    CScope&               GetScope(void) const;
    CConstRef<CDense_seg> Get()          const;

private:
    // Prohibit copy constructor and assignment operator
    CAlnMix(const CAlnMix& value);
    CAlnMix& operator=(const CAlnMix& value);

    void x_CreateScope              (void);
    bool x_MergeInit                (void);
    void x_Merge                    (void);

    CRef<CObjectManager>     m_ObjMgr;
    CRef<CScope>             m_Scope;
    TConstAlns               m_Alns;
    CRef<CDense_seg>         m_DS;
    TMergeFlags              m_MergeFlags;
};

///////////////////////////////////////////////////////////
///////////////////// inline methods //////////////////////
///////////////////////////////////////////////////////////

inline
CScope& CAlnMix::GetScope() const
{
    return *m_Scope;
}


inline
void CAlnMix::Add(const TAlns& alns)
{
    m_DS.Reset();
    iterate(TAlns, it, alns) {
        m_Alns.push_back(*it);
    }
}


inline
void CAlnMix::Add(CConstRef<CSeq_align> aln)
{
    m_DS.Reset();
    m_Alns.push_back(aln);
}


inline
void CAlnMix::Merge(TMergeFlags flags)
{
    if ( !m_DS  ||  m_MergeFlags != flags) {
        m_DS = null;
        m_MergeFlags = flags;
        x_Merge();
    }
}


inline
CConstRef<CDense_seg> CAlnMix::Get() const
{
    return m_DS;
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
* Revision 1.2  2002/10/08 18:02:09  todorov
* changed the aln lst input param
*
* Revision 1.1  2002/08/23 14:43:50  ucko
* Add the new C++ alignment manager to the public tree (thanks, Kamen!)
*
*
* ===========================================================================
*/

#endif // OBJECTS_ALNMGR___ALNMIX__HPP
