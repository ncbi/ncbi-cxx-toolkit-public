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
#include <objects/seq/Seq_descr.hpp>
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
class CSeqdesc;

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
    explicit CSeq_entry_Info(CSeq_entry& entry);
    virtual ~CSeq_entry_Info(void);

    const CBioseq_set_Info& GetParentBioseq_set_Info(void) const;
    CBioseq_set_Info& GetParentBioseq_set_Info(void);

    const CSeq_entry_Info& GetParentSeq_entry_Info(void) const;
    CSeq_entry_Info& GetParentSeq_entry_Info(void);


    typedef CSeq_entry TObject;

    bool HasSeq_entry(void) const;
    CConstRef<TObject> GetCompleteSeq_entry(void) const;
    CConstRef<TObject> GetSeq_entryCore(void) const;

    // Seq-entry access
    typedef TObject::E_Choice E_Choice;
    E_Choice Which(void) const;
    void x_CheckWhich(E_Choice which) const;
    void Reset(void);

    typedef CBioseq_set_Info TSet;
    bool IsSet(void) const;
    const TSet& GetSet(void) const;
    TSet& SetSet(void);

    // SelectSet switches Seq-entry to e_Set variant
    TSet& SelectSet(void);
    TSet& SelectSet(TSet& seqset);
    TSet& SelectSet(CBioseq_set& seqset);

    typedef CBioseq_Info TSeq;
    bool IsSeq(void) const;
    const TSeq& GetSeq(void) const;
    TSeq& SetSeq(void);

    // SelectSeq switches Seq-entry to e_Seq variant
    TSeq& SelectSeq(TSeq& seq);
    TSeq& SelectSeq(CBioseq& seq);

    typedef CSeq_descr TDescr;
    // Bioseq-set access
    bool IsSetDescr(void) const;
    const TDescr& GetDescr(void) const;
    void ResetDescr(void);
    void SetDescr(TDescr& v);
    bool AddSeqdesc(CSeqdesc& d);
    CRef<CSeqdesc> RemoveSeqdesc(const CSeqdesc& d);
    void AddDescr(CSeq_entry_Info& src);

    // low level access for CSeqdesc_CI in case sequence is split
    typedef CSeq_descr::Tdata::const_iterator TDesc_CI;
    typedef unsigned TDescTypeMask;
    bool x_IsEndDesc(TDesc_CI iter) const;
    TDesc_CI x_GetFirstDesc(TDescTypeMask types) const;
    TDesc_CI x_GetNextDesc(TDesc_CI iter, TDescTypeMask types) const;

    CRef<CSeq_annot_Info> AddAnnot(const CSeq_annot& annot);
    void AddAnnot(CRef<CSeq_annot_Info> annot);
    void RemoveAnnot(CRef<CSeq_annot_Info> annot);

    CRef<CSeq_entry_Info> AddEntry(CSeq_entry& entry, int index = -1);
    void AddEntry(CRef<CSeq_entry_Info> entry, int index = -1);
    void RemoveEntry(CRef<CSeq_entry_Info> entry);

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

protected:
    friend class CScope_Impl;
    friend class CDataSource;

    friend class CAnnot_Collector;
    friend class CSeq_annot_CI;

    friend class CTSE_Info;
    friend class CBioseq_Base_Info;
    friend class CBioseq_Info;
    friend class CBioseq_set_Info;
    friend class CSeq_annot_Info;

    void x_AttachContents(void);
    void x_DetachContents(void);

    TObject& x_GetObject(void);
    const TObject& x_GetObject(void) const;

    void x_SetObject(TObject& obj);
    void x_SetObject(const CSeq_entry_Info& info);

    void x_DetachObjectVariant(void);
    void x_AttachObjectVariant(CBioseq& seq);
    void x_AttachObjectVariant(CBioseq_set& seqset);

    void x_Select(CSeq_entry::E_Choice which,
                  CBioseq_Base_Info* contents);
    void x_Select(CSeq_entry::E_Choice which,
                  CRef<CBioseq_Base_Info> contents);

    virtual void x_DSMapObject(CConstRef<TObject> obj, CDataSource& ds);
    virtual void x_DSUnmapObject(CConstRef<TObject> obj, CDataSource& ds);

    void x_UpdateAnnotIndexContents(CTSE_Info& tse);

    void x_DoUpdate(TNeedUpdateFlags flags);
    void x_SetNeedUpdateContents(TNeedUpdateFlags flags);

    static CRef<TObject> sx_ShallowCopy(TObject& obj);

    // Seq-entry pointer
    CRef<TObject>           m_Object;

    // Bioseq/Bioseq_set info
    E_Choice                m_Which;
    CRef<CBioseq_Base_Info> m_Contents;

    // Hide copy methods
    CSeq_entry_Info& operator= (const CSeq_entry_Info&);
};



/////////////////////////////////////////////////////////////////////
//
//  Inline methods
//
/////////////////////////////////////////////////////////////////////


inline
bool CSeq_entry_Info::HasSeq_entry(void) const
{
    return m_Object.NotEmpty();
}


inline
CSeq_entry::E_Choice CSeq_entry_Info::Which(void) const
{
    return m_Which;
}


inline
CSeq_entry& CSeq_entry_Info::x_GetObject(void)
{
    return *m_Object;
}


inline
const CSeq_entry& CSeq_entry_Info::x_GetObject(void) const
{
    return *m_Object;
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
* Revision 1.18  2005/02/28 15:23:05  grichenk
* RemoveDesc() returns CRef<CSeqdesc>
*
* Revision 1.17  2005/01/12 17:16:14  vasilche
* Avoid performance warning on MSVC.
*
* Revision 1.16  2004/10/07 14:03:32  vasilche
* Use shared among TSEs CTSE_Split_Info.
* Use typedefs and methods for TSE and DataSource locking.
* Load split CSeqdesc on the fly in CSeqdesc_CI.
*
* Revision 1.15  2004/08/04 14:53:26  vasilche
* Revamped object manager:
* 1. Changed TSE locking scheme
* 2. TSE cache is maintained by CDataSource.
* 3. CObjectManager::GetInstance() doesn't hold CRef<> on the object manager.
* 4. Fixed processing of split data.
*
* Revision 1.14  2004/07/12 16:57:32  vasilche
* Fixed loading of split Seq-descr and Seq-data objects.
* They are loaded correctly now when GetCompleteXxx() method is called.
*
* Revision 1.13  2004/04/05 15:56:13  grichenk
* Redesigned CAnnotTypes_CI: moved all data and data collecting
* functions to CAnnotDataCollector. CAnnotTypes_CI is no more
* inherited from SAnnotSelector.
*
* Revision 1.12  2004/03/31 17:08:06  vasilche
* Implemented ConvertSeqToSet and ConvertSetToSeq.
*
* Revision 1.11  2004/03/25 19:27:44  vasilche
* Implemented MoveTo and CopyTo methods of handles.
*
* Revision 1.10  2004/03/24 18:30:29  vasilche
* Fixed edit API.
* Every *_Info object has its own shallow copy of original object.
*
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
