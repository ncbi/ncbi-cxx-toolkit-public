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


#include <objmgr/impl/tse_info_object.hpp>
#include <objects/seqset/Seq_entry.hpp>
#include <objects/seqset/Bioseq_set.hpp>
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
class CBioseq_Base_Info;
class CBioseq_set_Info;
class CBioseq_Info;
class CSeq_annot_Info;
class CSeq_descr;

////////////////////////////////////////////////////////////////////
//
//  CSeq_entry_Info::
//
//    General information and indexes for seq-entries
//


class NCBI_XOBJMGR_EXPORT CSeq_entry_Info : public CTSE_Info_Object
{
    typedef CTSE_Info_Object TParent;
public:
    // 'ctors
    CSeq_entry_Info(void);
    explicit CSeq_entry_Info(const CSeq_entry_Info& info);
    explicit CSeq_entry_Info(const CSeq_entry& entry);
    virtual ~CSeq_entry_Info(void);

    const CBioseq_set_Info& GetParentBioseq_set_Info(void) const;
    CBioseq_set_Info& GetParentBioseq_set_Info(void);

    const CSeq_entry_Info& GetParentSeq_entry_Info(void) const;
    CSeq_entry_Info& GetParentSeq_entry_Info(void);


    typedef CSeq_entry TObject;

    CConstRef<TObject> GetCompleteSeq_entry(void) const;
    CConstRef<TObject> GetSeq_entryCore(void) const;

    // Seq-entry access
    typedef TObject::E_Choice E_Choice;
    E_Choice Which(void) const;
    void x_CheckWhich(E_Choice which) const;

    typedef CBioseq_set_Info TSet;
    bool IsSet(void) const;
    const TSet& GetSet(void) const;
    TSet& SetSet(void);

    typedef CBioseq_Info TSeq;
    bool IsSeq(void) const;
    const TSeq& GetSeq(void) const;
    TSeq& SetSeq(void);

    typedef CSeq_descr TDescr;
    // Bioseq-set access
    bool IsSetDescr(void) const;
    const TDescr& GetDescr(void) const;

    // tree initialization
    void x_Attach(CRef<CSeq_entry_Info> sub_entry, int index = -1);
    void x_Attach(CRef<CBioseq_Info> bioseq);

    void x_ParentAttach(CBioseq_set_Info& parent);
    void x_ParentDetach(CBioseq_set_Info& parent);

    // attaching/detaching to CDataSource (it's in CTSE_Info)
    virtual void x_DSAttachContents(CDataSource& ds);
    virtual void x_DSDetachContents(CDataSource& ds);

    // attaching/detaching to CTSE_Info
    virtual void x_TSEAttachContents(CTSE_Info& tse_info);
    virtual void x_TSEDetachContents(CTSE_Info& tse_info);

    void UpdateAnnotIndex(void) const;

    CRef<CSeq_annot_Info> x_AddAnnot(const CSeq_annot& annot);
    void x_AddAnnot(CRef<CSeq_annot_Info> annot);
    void x_RemoveAnnot(CRef<CSeq_annot_Info> annot);

    CRef<CSeq_entry_Info> x_AddEntry(const CSeq_entry& entry, int index = -1);
    void x_AddEntry(CRef<CSeq_entry_Info> entry, int index = -1);
    void x_RemoveEntry(CRef<CSeq_entry_Info> entry);

    bool x_IsModified(void) const;
    void x_SetModifiedContents(void);
    void x_ResetModified(void);

protected:
    friend class CDataSource;
    friend class CAnnotTypes_CI;
    friend class CSeq_annot_CI;
    friend class CTSE_Info;
    friend class CSeq_annot_Info;
    friend class CBioseq_Info;
    friend class CScope_Impl;

    void x_AttachContents(void);
    void x_DetachContents(void);

    CRef<TObject> x_CreateObject(void) const;

    void x_SetObject(const TObject& obj);
    void x_UpdateModifiedObject(void) const;
    void x_UpdateObject(CConstRef<TObject> obj);

    typedef vector< CConstRef<TObject> > TDSMappedObjects;
    virtual void x_DSMapObject(CConstRef<TObject> obj, CDataSource& ds);
    virtual void x_DSUnmapObject(CConstRef<TObject> obj, CDataSource& ds);

    void x_UpdateAnnotIndexContents(CTSE_Info& tse);

    // Seq-entry pointer
    CConstRef<TObject>      m_Object;
    TDSMappedObjects        m_DSMappedObjects;

    // Bioseq/Bioseq_set info
    E_Choice                m_Which;
    CRef<CBioseq_Base_Info> m_Contents;

    // flags of modified elements
    typedef bool TModified;
    TModified               m_Modified;

    // Hide copy methods
    CSeq_entry_Info& operator= (const CSeq_entry_Info&);
};



/////////////////////////////////////////////////////////////////////
//
//  Inline methods
//
/////////////////////////////////////////////////////////////////////


inline
bool CSeq_entry_Info::x_IsModified(void) const
{
    return m_Modified;
}


inline
CSeq_entry::E_Choice CSeq_entry_Info::Which(void) const
{
    return m_Which;
}


inline
bool CSeq_entry_Info::IsSet(void) const
{
    return Which() == CSeq_entry::e_Set;
}


inline
bool CSeq_entry_Info::IsSeq(void) const
{
    return Which() == CSeq_entry::e_Seq;
}


END_SCOPE(objects)
END_NCBI_SCOPE

/*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.9  2004/03/16 15:47:27  vasilche
* Added CBioseq_set_Handle and set of EditHandles
*
* Revision 1.8  2004/02/03 19:02:16  vasilche
* Fixed broken 'dirty annot index' state after RemoveEntry().
*
* Revision 1.7  2003/12/18 16:38:06  grichenk
* Added CScope::RemoveEntry()
*
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
