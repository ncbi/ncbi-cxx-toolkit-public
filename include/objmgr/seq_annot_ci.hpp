#ifndef SEQ_ANNOT_CI__HPP
#define SEQ_ANNOT_CI__HPP

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
* Author: Aleksey Grichenko, Eugene Vasilchenko
*
* File Description:
*   Seq-annot iterator
*
*/


#include <corelib/ncbiobj.hpp>
#include <objmgr/scope.hpp>
#include <objmgr/seq_annot_handle.hpp>
#include <objmgr/seq_entry_handle.hpp>
#include <objmgr/seq_entry_ci.hpp>

#include <vector>
#include <stack>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

class CSeq_annot_Handle;
class CSeq_entry_Info;
class CSeq_annot_Info;
class CBioseq_set_Info;
class CBioseq_Handle;
class CBioseq_set_Handle;


class NCBI_XOBJMGR_EXPORT CSeq_annot_CI
{
public:
    enum EFlags {
        eSearch_entry,
        eSearch_recursive
    };

    CSeq_annot_CI(void);
    explicit CSeq_annot_CI(const CSeq_entry_Handle& entry,
                           EFlags flags = eSearch_recursive);
    explicit CSeq_annot_CI(const CBioseq_set_Handle& bioseq_set,
                           EFlags flags = eSearch_recursive);
    CSeq_annot_CI(CScope& scope, const CSeq_entry& entry,
                  EFlags flags = eSearch_recursive);
    // Iterate from the bioseq up to the TSE
    explicit CSeq_annot_CI(const CBioseq_Handle& bioseq);
    CSeq_annot_CI(const CSeq_annot_CI& iter);
    ~CSeq_annot_CI(void);

    CSeq_annot_CI& operator=(const CSeq_annot_CI& iter);

    operator bool(void) const;
    CSeq_annot_CI& operator++(void);

    CScope& GetScope(void) const;

    const CSeq_annot_Handle& operator*(void) const;
    const CSeq_annot_Handle* operator->(void) const;

    /*
    struct SEntryLevel_CI
    {
        typedef vector< CRef<CSeq_entry_Info> > TEntries;
        typedef TEntries::const_iterator  TEntry_CI;
        
        SEntryLevel_CI(const CBioseq_set_Info& seqset, const TEntry_CI& iter);
        SEntryLevel_CI(const SEntryLevel_CI&);
        SEntryLevel_CI& operator=(const SEntryLevel_CI&);
        ~SEntryLevel_CI(void);
        
        CConstRef<CBioseq_set_Info> m_Set;
        TEntry_CI                   m_Iter;
    };
    */
private:
    void x_Initialize(const CSeq_entry_Handle& entry_handle, EFlags flags);

    void x_SetEntry(const CSeq_entry_Handle& entry);
    void x_Push(void);
    void x_Settle(void);

    typedef vector< CRef<CSeq_annot_Info> > TAnnots;
    typedef TAnnots::const_iterator TAnnot_CI;
    typedef stack<CSeq_entry_CI> TEntryStack;

    const TAnnots& x_GetAnnots(void) const;

    CSeq_entry_Handle           m_CurrentEntry;
    TAnnot_CI                   m_AnnotIter;
    CSeq_annot_Handle           m_CurrentAnnot;
    TEntryStack                 m_EntryStack;
    // Used when initialized with a bioseq handle to iterate upwards
    bool                        m_UpTree;
};


/////////////////////////////////////////////////////////////////////
//
//  Inline methods
//
/////////////////////////////////////////////////////////////////////


inline
CScope& CSeq_annot_CI::GetScope(void) const
{
    return m_CurrentEntry.GetScope();
}


inline
CSeq_annot_CI::operator bool(void) const
{
    return m_CurrentAnnot;
}


inline
const CSeq_annot_Handle& CSeq_annot_CI::operator*(void) const
{
    _ASSERT(*this);
    return m_CurrentAnnot;
}


inline
const CSeq_annot_Handle* CSeq_annot_CI::operator->(void) const
{
    _ASSERT(*this);
    return &m_CurrentAnnot;
}


END_SCOPE(objects)
END_NCBI_SCOPE

/*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.10  2004/08/04 14:53:25  vasilche
* Revamped object manager:
* 1. Changed TSE locking scheme
* 2. TSE cache is maintained by CDataSource.
* 3. CObjectManager::GetInstance() doesn't hold CRef<> on the object manager.
* 4. Fixed processing of split data.
*
* Revision 1.9  2004/04/26 14:13:45  grichenk
* Added constructors from bioseq-set handle and bioseq handle.
*
* Revision 1.8  2004/03/16 15:47:26  vasilche
* Added CBioseq_set_Handle and set of EditHandles
*
* Revision 1.7  2003/10/08 17:55:53  vasilche
* Fixed null initialization of CHeapScope.
*
* Revision 1.6  2003/10/08 14:14:53  vasilche
* Use CHeapScope instead of CRef<CScope> internally.
*
* Revision 1.5  2003/09/30 16:21:59  vasilche
* Updated internal object manager classes to be able to load ID2 data.
* SNP blobs are loaded as ID2 split blobs - readers convert them automatically.
* Scope caches results of requests for data to data loaders.
* Optimized CSeq_id_Handle for gis.
* Optimized bioseq lookup in scope.
* Reduced object allocations in annotation iterators.
* CScope is allowed to be destroyed before other objects using this scope are
* deleted (feature iterators, bioseq handles etc).
* Optimized lookup for matching Seq-ids in CSeq_id_Mapper.
* Added 'adaptive' option to objmgr_demo application.
*
* Revision 1.4  2003/08/22 14:59:25  grichenk
* + operators ==, !=, <
*
* Revision 1.3  2003/08/04 17:02:57  grichenk
* Added constructors to iterate all annotations from a
* seq-entry or seq-annot.
*
* Revision 1.2  2003/07/25 21:41:27  grichenk
* Implemented non-recursive mode for CSeq_annot_CI,
* fixed friend declaration in CSeq_entry_Info.
*
* Revision 1.1  2003/07/25 15:23:41  grichenk
* Initial revision
*
*
* ===========================================================================
*/

#endif  // SEQ_ANNOT_CI__HPP
