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
    explicit CSeq_entry_Info(const CSeq_entry_Info& info,
                             TObjectCopyMap* copy_map);
    explicit CSeq_entry_Info(CSeq_entry& entry);
    virtual ~CSeq_entry_Info(void);

    const CBioseq_set_Info& GetParentBioseq_set_Info(void) const;
    CBioseq_set_Info& GetParentBioseq_set_Info(void);

    const CSeq_entry_Info& GetParentSeq_entry_Info(void) const;
    CSeq_entry_Info& GetParentSeq_entry_Info(void);

    // Get unique bio object id
    virtual const CBioObjectId& GetBioObjectId(void) const;

    typedef CSeq_entry TObject;

    bool HasSeq_entry(void) const;
    CConstRef<TObject> GetCompleteSeq_entry(void) const;
    CConstRef<TObject> GetSeq_entryCore(void) const;
    CConstRef<TObject> GetSeq_entrySkeleton(void) const;

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
    //    void AddDescr(CSeq_entry_Info& src);
    void AddSeq_descr(const TDescr& v);

    // get current content no matter what type it is
    const CBioseq_Base_Info& x_GetBaseInfo(void) const;

    // low level access for CSeqdesc_CI in case sequence is split
    typedef CSeq_descr::Tdata::const_iterator TDesc_CI;
    typedef unsigned TDescTypeMask;
    bool x_IsEndDesc(TDesc_CI iter) const;
    TDesc_CI x_GetFirstDesc(TDescTypeMask types) const;
    TDesc_CI x_GetNextDesc(TDesc_CI iter, TDescTypeMask types) const;

    CRef<CSeq_annot_Info> AddAnnot(CSeq_annot& annot);
    void AddAnnot(CRef<CSeq_annot_Info> annot);
    void RemoveAnnot(CRef<CSeq_annot_Info> annot);

    typedef vector< CRef<CSeq_annot_Info> > TAnnot;
    const TAnnot& GetLoadedAnnot(void) const;

    CRef<CSeq_entry_Info> AddEntry(CSeq_entry& entry, int index = -1);
    void AddEntry(CRef<CSeq_entry_Info> entry, int index = -1);
    void RemoveEntry(CRef<CSeq_entry_Info> entry);

    // tree initialization
    void x_ParentAttach(CBioseq_set_Info& parent);
    void x_ParentDetach(CBioseq_set_Info& parent);

    // attaching/detaching to CDataSource (it's in CTSE_Info)
    virtual void x_DSAttachContents(CDataSource& ds);
    virtual void x_DSDetachContents(CDataSource& ds);

    // attaching/detaching to CTSE_Info
    virtual void x_TSEAttachContents(CTSE_Info& tse_info);
    virtual void x_TSEDetachContents(CTSE_Info& tse_info);

    void UpdateAnnotIndex(void) const;

    void x_SetBioseqChunkId(TChunkId chunk_id);

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
    void x_SetObject(const CSeq_entry_Info& info, TObjectCopyMap* copy_map);

    void x_Select(CSeq_entry::E_Choice which,
                  CBioseq_Base_Info* contents);
    void x_Select(CSeq_entry::E_Choice which,
                  CRef<CBioseq_Base_Info> contents);

    void x_DSMapObject(CConstRef<TObject> obj, CDataSource& ds);
    void x_DSUnmapObject(CConstRef<TObject> obj, CDataSource& ds);

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
CConstRef<CSeq_entry> CSeq_entry_Info::GetSeq_entrySkeleton(void) const
{
    return m_Object;   
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
const CBioseq_Base_Info& CSeq_entry_Info::x_GetBaseInfo(void) const
{
    return *m_Contents;
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

#endif  /* OBJECTS_OBJMGR_IMPL___SEQ_ENTRY_INFO__HPP */
