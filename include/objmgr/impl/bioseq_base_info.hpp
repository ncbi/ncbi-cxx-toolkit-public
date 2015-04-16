#ifndef OBJECTS_OBJMGR_IMPL___BIOSEQ_BASE_INFO__HPP
#define OBJECTS_OBJMGR_IMPL___BIOSEQ_BASE_INFO__HPP

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
 *   Bioseq info for data source
 *
 */

#include <corelib/ncbiobj.hpp>
#include <objmgr/impl/tse_info_object.hpp>

#include <objects/seq/seq_id_handle.hpp>
#include <objects/seq/Seq_descr.hpp>

#include <vector>
#include <list>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

class CDataSource;
class CTSE_Info;
class CSeq_entry;
class CSeq_entry_Info;
class CSeq_annot;
class CSeq_annot_Info;
class CSeq_descr;
class CSeqdesc;

////////////////////////////////////////////////////////////////////
//
//  CBioseq_Info::
//
//    Structure to keep bioseq's parent seq-entry along with the list
//    of seq-id synonyms for the bioseq.
//


class NCBI_XOBJMGR_EXPORT CBioseq_Base_Info : public CTSE_Info_Object
{
    typedef CTSE_Info_Object TParent;
public:
    // 'ctors
    CBioseq_Base_Info(void);
    CBioseq_Base_Info(const CBioseq_Base_Info& src, TObjectCopyMap* copy_map);
    virtual ~CBioseq_Base_Info(void);

    // info tree
    const CSeq_entry_Info& GetParentSeq_entry_Info(void) const;
    CSeq_entry_Info& GetParentSeq_entry_Info(void);

    // member modification
    // descr
    typedef CSeq_descr TDescr;
    bool IsSetDescr(void) const;
    bool CanGetDescr(void) const;
    const TDescr& GetDescr(void) const;
    TDescr& SetDescr(void);
    void SetDescr(TDescr& v);
    void ResetDescr(void);
    bool AddSeqdesc(CSeqdesc& d);
    CRef<CSeqdesc> RemoveSeqdesc(const CSeqdesc& d);
    void AddSeq_descr(const TDescr& v);

    virtual bool x_IsSetDescr(void) const = 0;
    virtual bool x_CanGetDescr(void) const = 0;
    virtual const TDescr& x_GetDescr(void) const = 0;
    virtual TDescr& x_SetDescr(void) = 0;
    virtual void x_SetDescr(TDescr& v) = 0;
    virtual void x_ResetDescr(void) = 0;


    // low level access for CSeqdesc_CI in case sequence is split
    typedef TDescr::Tdata TDescList;
    typedef TDescList::const_iterator TDesc_CI;
    typedef unsigned TDescTypeMask;
    
    const TDescList& x_GetDescList(void) const;
    TDesc_CI x_GetFirstDesc(TDescTypeMask types) const;
    TDesc_CI x_GetNextDesc(TDesc_CI iter, TDescTypeMask types) const;
    bool x_IsEndDesc(TDesc_CI iter) const;
    TDesc_CI x_FindDesc(TDesc_CI iter, TDescTypeMask types) const;
    void x_PrefetchDesc(TDesc_CI last, TDescTypeMask types) const;

    const CSeqdesc* x_SearchFirstDesc(TDescTypeMask type) const;

    // annot
    typedef vector< CRef<CSeq_annot_Info> > TAnnot;
    typedef list< CRef<CSeq_annot> > TObjAnnot;
    bool IsSetAnnot(void) const;
    bool HasAnnots(void) const;
    const TAnnot& GetAnnot(void) const;
    const TAnnot& GetLoadedAnnot(void) const;

    void ResetAnnot(void);
    CRef<CSeq_annot_Info> AddAnnot(CSeq_annot& annot, int chunk_id = 0);
    void AddAnnot(CRef<CSeq_annot_Info> annot);
    void RemoveAnnot(CRef<CSeq_annot_Info> annot);

    // object initialization
    void x_AttachAnnot(CRef<CSeq_annot_Info> info);
    void x_DetachAnnot(CRef<CSeq_annot_Info> info);

    // info tree initialization
    virtual void x_DSAttachContents(CDataSource& ds);
    virtual void x_DSDetachContents(CDataSource& ds);

    virtual void x_TSEAttachContents(CTSE_Info& tse);
    virtual void x_TSEDetachContents(CTSE_Info& tse);

    virtual void x_ParentAttach(CSeq_entry_Info& parent);
    virtual void x_ParentDetach(CSeq_entry_Info& parent);

    // index support
    void x_UpdateAnnotIndexContents(CTSE_Info& tse);

    void x_SetAnnot(void);
    void x_SetAnnot(const CBioseq_Base_Info& info, TObjectCopyMap* copy_map);

    void x_AddDescrChunkId(const TDescTypeMask& types, TChunkId chunk_id);
    void x_AddAnnotChunkId(TChunkId chunk_id);

    virtual TObjAnnot& x_SetObjAnnot(void) = 0;
    virtual void x_ResetObjAnnot(void) = 0;

    void x_DoUpdate(TNeedUpdateFlags flags);
    void x_SetNeedUpdateParent(TNeedUpdateFlags flags);

private:
    bool x_IsEndNextDesc(TDesc_CI iter) const; // internal inlined method

    friend class CAnnotTypes_CI;
    friend class CSeq_annot_CI;

    // members
    TAnnot              m_Annot;
    TObjAnnot*          m_ObjAnnot;

    TChunkIds           m_DescrChunks;
    typedef vector<TDescTypeMask> TDescTypeMasks;
    TDescTypeMasks      m_DescrTypeMasks;
    TChunkIds           m_AnnotChunks;

};



/////////////////////////////////////////////////////////////////////
//
//  Inline methods
//
/////////////////////////////////////////////////////////////////////


inline
bool CBioseq_Base_Info::IsSetDescr(void) const
{
    return x_NeedUpdate(fNeedUpdate_descr) || x_IsSetDescr();
}


inline
bool CBioseq_Base_Info::CanGetDescr(void) const
{
    return x_NeedUpdate(fNeedUpdate_descr) || x_CanGetDescr();
}


inline
bool CBioseq_Base_Info::IsSetAnnot(void) const
{
    return m_ObjAnnot != 0 || x_NeedUpdate(fNeedUpdate_annot);
}


inline
bool CBioseq_Base_Info::HasAnnots(void) const
{
    return !m_Annot.empty() || x_NeedUpdate(fNeedUpdate_annot);
}


inline
const CBioseq_Base_Info::TAnnot& CBioseq_Base_Info::GetAnnot(void) const
{
    x_Update(fNeedUpdate_annot);
    return m_Annot;
}

inline
const CBioseq_Base_Info::TAnnot& CBioseq_Base_Info::GetLoadedAnnot(void) const
{
    return m_Annot;
}


END_SCOPE(objects)
END_NCBI_SCOPE

#endif//OBJECTS_OBJMGR_IMPL___BIOSEQ_BASE_INFO__HPP
