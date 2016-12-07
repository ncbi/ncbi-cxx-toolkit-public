#ifndef OBJECTS_OBJMGR_IMPL___BIOSEQ_SET_INFO__HPP
#define OBJECTS_OBJMGR_IMPL___BIOSEQ_SET_INFO__HPP

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


#include <objmgr/impl/bioseq_base_info.hpp>
#include <objects/seqset/Bioseq_set.hpp>
#include <objects/general/Object_id.hpp>
#include <objects/general/Dbtag.hpp>
#include <objects/general/Date.hpp>
#include <vector>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

// forward declaration
class CBioseq_set;
class CSeq_entry_Info;

////////////////////////////////////////////////////////////////////
//
//  CBioseq_set_Info::
//
//    General information and indexes for Bioseq-set object
//


class NCBI_XOBJMGR_EXPORT CBioseq_set_Info : public CBioseq_Base_Info
{
    typedef CBioseq_Base_Info TParent;
public:
    // 'ctors
    CBioseq_set_Info(void);
    explicit CBioseq_set_Info(const CBioseq_set_Info& info,
                              TObjectCopyMap* copy_map);
    explicit CBioseq_set_Info(CBioseq_set& seqset);
    virtual ~CBioseq_set_Info(void);

    typedef CBioseq_set TObject;

    CConstRef<TObject> GetCompleteBioseq_set(void) const;
    CConstRef<TObject> GetBioseq_setCore(void) const;

    bool IsEmptySeq_set(void) const;

    // Bioseq-set access
    typedef TObject::TId TId;
    bool IsSetId(void) const;
    bool CanGetId(void) const;
    const TId& GetId(void) const;
    void SetId(TId& v);
    void ResetId(void);

    bool x_IsSetDescr(void) const;
    bool x_CanGetDescr(void) const;
    const TDescr& x_GetDescr(void) const;
    TDescr& x_SetDescr(void);
    void x_SetDescr(TDescr& v);
    void x_ResetDescr(void);

    typedef TObject::TColl TColl;
    bool IsSetColl(void) const;
    bool CanGetColl(void) const;
    const TColl& GetColl(void) const;
    void SetColl(TColl& v);
    void ResetColl(void);

    typedef TObject::TLevel TLevel;
    bool IsSetLevel(void) const;
    bool CanGetLevel(void) const;
    TLevel GetLevel(void) const;
    void SetLevel(TLevel v);
    void ResetLevel(void);

    typedef TObject::TClass TClass;
    bool IsSetClass(void) const;
    bool CanGetClass(void) const;
    TClass GetClass(void) const;
    void SetClass(TClass v);
    void ResetClass(void);

    typedef TObject::TRelease TRelease;
    bool IsSetRelease(void) const;
    bool CanGetRelease(void) const;
    const TRelease& GetRelease(void) const;
    void SetRelease(TRelease& v);
    void ResetRelease(void);

    typedef TObject::TDate TDate;
    bool IsSetDate(void) const;
    bool CanGetDate(void) const;
    const TDate& GetDate(void) const;
    void SetDate(TDate& v);
    void ResetDate(void);

    typedef vector< CRef<CSeq_entry_Info> > TSeq_set;
    bool IsSetSeq_set(void) const;
    bool CanGetSeq_set(void) const;
    const TSeq_set& GetSeq_set(void) const;
    TSeq_set& SetSeq_set(void);

    // return first already loaded Seq-entry or null
    CConstRef<CSeq_entry_Info> GetFirstEntry(void) const;

    CRef<CSeq_entry_Info> AddEntry(CSeq_entry& entry, int index, // index == -1 -> at the end
                                   bool set_uniqid = false);
    void AddEntry(CRef<CSeq_entry_Info> entry, int index = -1, 
                  bool set_uniqid = false);
    void RemoveEntry(CRef<CSeq_entry_Info> entry);
    // returns -1 if entry is not found
    int GetEntryIndex(const CSeq_entry_Info& entry) const;

    // initialization
    // attaching/detaching to CDataSource (it's in CTSE_Info)
    virtual void x_DSAttachContents(CDataSource& ds);
    virtual void x_DSDetachContents(CDataSource& ds);

    void x_AddBioseqChunkId(TChunkId chunk_id);

    // attaching/detaching to CTSE_Info
    virtual void x_TSEAttachContents(CTSE_Info& tse);
    virtual void x_TSEDetachContents(CTSE_Info& tse);

    // index
    void UpdateAnnotIndex(void) const;
    virtual void x_UpdateAnnotIndexContents(CTSE_Info& tse);
    
    // modification
    void x_AttachEntry(CRef<CSeq_entry_Info> info);
    void x_DetachEntry(CRef<CSeq_entry_Info> info);

    void x_SetChunkBioseqs(const list< CRef<CBioseq> >& bioseqs, int chunk_id);
    
protected:
    friend class CDataSource;
    friend class CScope_Impl;

    friend class CTSE_Info;
    friend class CSeq_entry_Info;
    friend class CBioseq_Info;
    friend class CSeq_annot_Info;

    friend class CSeq_entry_CI;
    friend class CSeq_entry_I;
    friend class CSeq_annot_CI;
    friend class CSeq_annot_I;
    friend class CAnnotTypes_CI;

    void x_DSAttachContents(void);
    void x_DSDetachContents(void);

    void x_ParentAttach(CSeq_entry_Info& parent);
    void x_ParentDetach(CSeq_entry_Info& parent);

    TObject& x_GetObject(void);
    const TObject& x_GetObject(void) const;

    void x_SetObject(TObject& obj);
    void x_SetObject(const CBioseq_set_Info& info, TObjectCopyMap* copy_map);

    void x_DSMapObject(CConstRef<TObject> obj, CDataSource& ds);
    void x_DSUnmapObject(CConstRef<TObject> obj, CDataSource& ds);

    int x_GetBioseq_set_Id(const CObject_id& object_id);

    TObjAnnot& x_SetObjAnnot(void);
    void x_ResetObjAnnot(void);

    void x_DoUpdate(TNeedUpdateFlags flags);

    static CRef<TObject> sx_ShallowCopy(const TObject& obj);

private:
    // core object
    CRef<TObject>       m_Object;

    // members
    TSeq_set            m_Seq_set;

    struct SChunkSeqSet {
        SChunkSeqSet() : count(0) {}
        
        size_t count;
        CBioseq_set::TSeq_set::iterator first_iter;
    };
    typedef map<TChunkId, SChunkSeqSet> TChunkSeqSets;
    TChunkSeqSets m_ChunkSeqSets;

    //
    TChunkIds           m_BioseqChunks;

    // index information
    int                 m_Bioseq_set_Id;
};



/////////////////////////////////////////////////////////////////////
//
//  Inline methods
//
/////////////////////////////////////////////////////////////////////


inline
CBioseq_set& CBioseq_set_Info::x_GetObject(void)
{
    return *m_Object;
}


inline
const CBioseq_set& CBioseq_set_Info::x_GetObject(void) const
{
    return *m_Object;
}


inline
bool CBioseq_set_Info::IsSetId(void) const
{
    return m_Object->IsSetId();
}


inline
bool CBioseq_set_Info::CanGetId(void) const
{
    return m_Object  &&  m_Object->CanGetId();
}


inline
void CBioseq_set_Info::ResetId(void)
{
    m_Object->ResetId();
}


inline
const CBioseq_set_Info::TId& CBioseq_set_Info::GetId(void) const
{
    return m_Object->GetId();
}


inline
void CBioseq_set_Info::SetId(TId& v)
{
    m_Object->SetId(v);
}


inline
bool CBioseq_set_Info::IsSetColl(void) const
{
    return m_Object->IsSetColl();
}


inline
bool CBioseq_set_Info::CanGetColl(void) const
{
    return m_Object  &&  m_Object->CanGetColl();
}


inline
void CBioseq_set_Info::ResetColl(void)
{
    m_Object->ResetColl();
}


inline
const CBioseq_set_Info::TColl& CBioseq_set_Info::GetColl(void) const
{
    return m_Object->GetColl();
}


inline
void CBioseq_set_Info::SetColl(TColl& v)
{
    m_Object->SetColl(v);
}


inline
bool CBioseq_set_Info::IsSetLevel(void) const
{
    return m_Object->IsSetLevel();
}


inline
bool CBioseq_set_Info::CanGetLevel(void) const
{
    return m_Object  &&  m_Object->CanGetLevel();
}


inline
void CBioseq_set_Info::ResetLevel(void)
{
    m_Object->ResetLevel();
}


inline
CBioseq_set_Info::TLevel CBioseq_set_Info::GetLevel(void) const
{
    return m_Object->GetLevel();
}


inline
void CBioseq_set_Info::SetLevel(TLevel v)
{
    m_Object->SetLevel(v);
}


inline
bool CBioseq_set_Info::IsSetClass(void) const
{
    return m_Object->IsSetClass();
}


inline
bool CBioseq_set_Info::CanGetClass(void) const
{
    return m_Object  &&  m_Object->CanGetClass();
}


inline
void CBioseq_set_Info::ResetClass(void)
{
    m_Object->ResetClass();
}


inline
CBioseq_set_Info::TClass CBioseq_set_Info::GetClass(void) const
{
    return m_Object->GetClass();
}


inline
void CBioseq_set_Info::SetClass(TClass v)
{
    m_Object->SetClass(v);
}


inline
bool CBioseq_set_Info::IsSetRelease(void) const
{
    return m_Object->IsSetRelease();
}


inline
bool CBioseq_set_Info::CanGetRelease(void) const
{
    return m_Object  &&  m_Object->CanGetRelease();
}


inline
void CBioseq_set_Info::ResetRelease(void)
{
    m_Object->ResetRelease();
}


inline
const CBioseq_set_Info::TRelease& CBioseq_set_Info::GetRelease(void) const
{
    return m_Object->GetRelease();
}


inline
void CBioseq_set_Info::SetRelease(TRelease& v)
{
    m_Object->SetRelease(v);
}


inline
bool CBioseq_set_Info::IsSetDate(void) const
{
    return m_Object->IsSetDate();
}


inline
bool CBioseq_set_Info::CanGetDate(void) const
{
    return m_Object  &&  m_Object->CanGetDate();
}


inline
void CBioseq_set_Info::ResetDate(void)
{
    m_Object->ResetDate();
}


inline
const CBioseq_set_Info::TDate& CBioseq_set_Info::GetDate(void) const
{
    return m_Object->GetDate();
}


inline
void CBioseq_set_Info::SetDate(TDate& v)
{
    m_Object->SetDate(v);
}


inline
bool CBioseq_set_Info::IsSetSeq_set(void) const
{
    return m_Object->IsSetSeq_set() || x_NeedUpdate(fNeedUpdate_bioseq);
}


inline
bool CBioseq_set_Info::CanGetSeq_set(void) const
{
    return m_Object  &&
        (m_Object->CanGetSeq_set() || x_NeedUpdate(fNeedUpdate_bioseq));
}


inline
const CBioseq_set_Info::TSeq_set& CBioseq_set_Info::GetSeq_set(void) const
{
    x_Update(fNeedUpdate_bioseq);
    return m_Seq_set;
}


inline
CBioseq_set_Info::TSeq_set& CBioseq_set_Info::SetSeq_set(void)
{
    x_Update(fNeedUpdate_bioseq);
    return m_Seq_set;
}


inline
bool CBioseq_set_Info::IsEmptySeq_set(void) const
{
    return !x_NeedUpdate(fNeedUpdate_bioseq) &&
        (!IsSetSeq_set() || GetSeq_set().empty());
}


END_SCOPE(objects)
END_NCBI_SCOPE

#endif//OBJECTS_OBJMGR_IMPL___BIOSEQ_SET_INFO__HPP
