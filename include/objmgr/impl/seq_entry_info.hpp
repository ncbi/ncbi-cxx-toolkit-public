#ifndef OBJECTS_OBJMGR_IMPL___SEQ_ENTRY_INFO__HPP
#define OBJECTS_OBJMGR_IMPL___SEQ_ENTRY_INFO__HPP

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
* Author: Eugene Vasilchenko
*
* File Description:
*   Seq_entry info -- entry for data source
*
*/


#include <corelib/ncbiobj.hpp>
#include <vector>
#include <list>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)


// forward declaration
class CSeq_entry;
class CBioseq;
class CBioseq_set;
class CSeq_annot;

class CDataSource;
class CTSE_Info;
class CSeq_entry_Info;
class CBioseq_Info;
class CSeq_annot_Info;

////////////////////////////////////////////////////////////////////
//
//  CSeq_entry_Info::
//
//    General information and indexes for seq-entries
//


class NCBI_XOBJMGR_EXPORT CSeq_entry_Info : public CObject
{
public:
    // 'ctors
    CSeq_entry_Info(CSeq_entry& entry, CSeq_entry_Info& parent);
    virtual ~CSeq_entry_Info(void);

    bool HaveDataSource(void) const;
    CDataSource& GetDataSource(void) const;

    const CSeq_entry& GetTSE(void) const;
    CSeq_entry& GetTSE(void);

    const CTSE_Info& GetTSE_Info(void) const;
    CTSE_Info& GetTSE_Info(void);

    const CSeq_entry_Info* GetParentSeq_entry_Info(void) const;

    bool NullSeq_entry(void) const;
    const CSeq_entry& GetSeq_entry(void) const;
    CSeq_entry& GetSeq_entry(void);

    const CBioseq_Info& GetBioseq_Info(void) const;
    CBioseq_Info& GetBioseq_Info(void);

    typedef vector< CRef<CSeq_entry_Info> > TEntries;
    typedef vector< CRef<CSeq_annot_Info> > TAnnots;

    // attaching/detaching to CDataSource (it's in CTSE_Info)
    void x_DSAttach(void);
    void x_DSDetach(void);

    // attaching/detaching to CTSE_Info
    void x_TSEAttach(CTSE_Info* tse_info);
    void x_TSEDetach(CTSE_Info* tse_info);

    void x_TSEAttach(void);
    void x_TSEDetach(void);
    
    void UpdateAnnotIndex(void) const;

    void x_AddAnnot(CSeq_annot& annot);
    void x_RemoveAnnot(CSeq_annot_Info& annot_info);

    void x_AddEntry(CSeq_entry& entry);
    void x_RemoveEntry(CSeq_entry_Info& entry_info);

protected:
    friend class CDataSource;
    friend class CAnnotTypes_CI;
    friend class CSeq_annot_CI;
    friend class CTSE_Info;
    friend class CSeq_annot_Info;
    friend class CBioseq_Info;
    friend class CSeq_entry_CI;

    CSeq_entry_Info(void);

    void x_DSAttachThis(void);
    void x_DSDetachThis(void);
    void x_DSAttachContents(void);
    void x_DSDetachContents(void);

    void x_SetSeq_entry(CSeq_entry& entry);

    void x_TSEDetachThis(void);
    void x_TSEDetachContents(void);
    void x_TSEAttachBioseq_set(CBioseq_set& seq_set);

    void x_TSEAttachBioseq_set_Id(const CBioseq_set& seq_set);
    void x_TSEDetachBioseq_set_Id(void);

    typedef list<CRef<CSeq_annot> > TSeq_annots;
    void x_TSEAttachSeq_annots(TSeq_annots& annots);

    void x_UpdateAnnotIndex(void);
    void x_UpdateAnnotIndexContents(void);

    void x_SetDirtyAnnotIndex(void);

    // Seq-entry pointer
    CRef<CSeq_entry>      m_Seq_entry;

    // parent Seq-entry info
    CSeq_entry_Info*      m_Parent;

    // top level Seq-entry info
    CTSE_Info*            m_TSE_Info;

    // children Seq-entry objects if Which() == e_Set
    TEntries              m_Entries;
    // children Bioseq object if Which() == e_Seq
    CRef<CBioseq_Info>    m_Bioseq;
    // Seq-annot objects
    TAnnots               m_Annots;

    int                   m_Bioseq_set_Id;

    bool                  m_DirtyAnnotIndex;

    // Hide copy methods
    CSeq_entry_Info(const CSeq_entry_Info&);
    CSeq_entry_Info& operator= (const CSeq_entry_Info&);
};



/////////////////////////////////////////////////////////////////////
//
//  Inline methods
//
/////////////////////////////////////////////////////////////////////


inline
const CTSE_Info& CSeq_entry_Info::GetTSE_Info(void) const
{
    return *m_TSE_Info;
}


inline
CTSE_Info& CSeq_entry_Info::GetTSE_Info(void)
{
    return *m_TSE_Info;
}


inline
const CSeq_entry_Info* CSeq_entry_Info::GetParentSeq_entry_Info(void) const
{
    return m_Parent;
}


inline
bool CSeq_entry_Info::NullSeq_entry(void) const
{
    return m_Seq_entry;
}


inline
const CSeq_entry& CSeq_entry_Info::GetSeq_entry(void) const
{
    return *m_Seq_entry;
}


inline
CSeq_entry& CSeq_entry_Info::GetSeq_entry(void)
{
    return *m_Seq_entry;
}


inline
const CBioseq_Info& CSeq_entry_Info::GetBioseq_Info(void) const
{
    return *m_Bioseq;
}


inline
CBioseq_Info& CSeq_entry_Info::GetBioseq_Info(void)
{
    return *m_Bioseq;
}


inline
void CSeq_entry_Info::x_DSDetach(void)
{
    x_DSDetachContents();
    x_DSDetachThis();
}


END_SCOPE(objects)
END_NCBI_SCOPE

/*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.6  2003/11/28 15:13:25  grichenk
* Added CSeq_entry_Handle
*
* Revision 1.5  2003/09/30 16:22:01  vasilche
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
* Revision 1.4  2003/08/04 17:02:59  grichenk
* Added constructors to iterate all annotations from a
* seq-entry or seq-annot.
*
* Revision 1.3  2003/07/25 21:41:29  grichenk
* Implemented non-recursive mode for CSeq_annot_CI,
* fixed friend declaration in CSeq_entry_Info.
*
* Revision 1.2  2003/07/25 15:25:24  grichenk
* Added CSeq_annot_CI class
*
* Revision 1.1  2003/04/24 16:12:37  vasilche
* Object manager internal structures are splitted more straightforward.
* Removed excessive header dependencies.
*
*
* ===========================================================================
*/

#endif  /* OBJECTS_OBJMGR_IMPL___SEQ_ENTRY_INFO__HPP */
