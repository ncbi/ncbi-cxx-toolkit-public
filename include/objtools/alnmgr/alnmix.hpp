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

    typedef vector< CConstRef< CDense_seg > > TConstDSs;
    

    // constructor
    CAlnMix(void);
    CAlnMix(CScope& scope);

    // destructor
    ~CAlnMix(void);

    void Add(const CDense_seg& ds);

    enum EMergeFlags {
        fGen2EST              = 0x01, // otherwise Nucl2Nucl
        fTruncateOverlaps     = 0x02, // otherwise put on separate rows
        fNegativeStrand       = 0x04,
        fTryOtherMethodOnFail = 0x08,
        fGapJoin              = 0x10
    };
    typedef int TMergeFlags; // binary OR of EMergeFlags
    void Merge(TMergeFlags flags = 0);

    CScope&            GetScope(void)  const;
    const CDense_seg&  GetDenseg(void) const;

private:
    // Prohibit copy constructor and assignment operator
    CAlnMix(const CAlnMix& value);
    CAlnMix& operator=(const CAlnMix& value);

    void x_CreateScope              (void);
    bool x_MergeInit                (void);
    void x_Merge                    (void);

    CRef<CObjectManager>                 m_ObjMgr;
    CRef<CScope>                         m_Scope;
    TConstDSs                            m_InputDSs;
    CRef<CDense_seg>                     m_DS;
    TMergeFlags                          m_MergeFlags;
};

///////////////////////////////////////////////////////////
///////////////////// inline methods //////////////////////
///////////////////////////////////////////////////////////

inline
CScope& CAlnMix::GetScope() const
{
    return const_cast<CScope&>(*m_Scope);
}


inline
void CAlnMix::Add(const CDense_seg &ds)
{
    m_DS.Reset();
    m_InputDSs.push_back(&ds);
}


inline
const CDense_seg& CAlnMix::GetDenseg() const
{
    if (!m_DS) {
        /* throw */
    }
    return *m_DS;
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
* Revision 1.5  2002/11/04 21:28:57  grichenk
* Fixed usage of const CRef<> and CRef<> constructor
*
* Revision 1.4  2002/10/25 20:02:20  todorov
* new fTryOtherMethodOnFail flag
*
* Revision 1.3  2002/10/24 21:31:33  todorov
* adding Dense-segs instead of Seq-aligns
*
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
