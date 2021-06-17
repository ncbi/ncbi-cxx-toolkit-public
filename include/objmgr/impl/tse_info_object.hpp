#ifndef OBJECTS_OBJMGR_IMPL___TSE_INFO_OBJECT__HPP
#define OBJECTS_OBJMGR_IMPL___TSE_INFO_OBJECT__HPP

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

#include <objmgr/bio_object_id.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

class CDataSource;
class CTSE_Info;
class CSeq_entry;
class CSeq_entry_Info;
class CSeq_annot;
class CSeq_annot_Info;
class CSeq_descr;

////////////////////////////////////////////////////////////////////
//
//  CTSE_Info_Object::
//
//    Structure to keep bioseq's parent seq-entry along with the list
//    of seq-id synonyms for the bioseq.
//


class NCBI_XOBJMGR_EXPORT CTSE_Info_Object : public CObject
{
public:
    typedef map<CConstRef<CObject>, CRef<CObject> > TObjectCopyMap;
    // 'ctors
    CTSE_Info_Object(void);
    CTSE_Info_Object(const CTSE_Info_Object& src, TObjectCopyMap* copy_map);
    virtual ~CTSE_Info_Object(void);

    // Get unique bio object id
    virtual const CBioObjectId& GetBioObjectId(void) const;
    virtual void SetBioObjectId(const CBioObjectId& id);

    // info tree
    bool HasDataSource(void) const;
    CDataSource& GetDataSource(void) const;

    bool HasTSE_Info(void) const;
    bool BelongsToTSE_Info(const CTSE_Info& tse) const;
    const CTSE_Info& GetTSE_Info(void) const;
    CTSE_Info& GetTSE_Info(void);

    bool HasParent_Info(void) const;
    const CTSE_Info_Object& GetBaseParent_Info(void) const;
    CTSE_Info_Object& GetBaseParent_Info(void);

    // info tree initialization
    void x_DSAttach(CDataSource& ds);
    void x_DSDetach(CDataSource& ds);

    virtual void x_DSAttachContents(CDataSource& ds);
    virtual void x_DSDetachContents(CDataSource& ds);

    void x_TSEAttach(CTSE_Info& tse);
    void x_TSEDetach(CTSE_Info& tse);

    virtual void x_TSEAttachContents(CTSE_Info& tse);
    virtual void x_TSEDetachContents(CTSE_Info& tse);

    // index support
    bool x_DirtyAnnotIndex(void) const;
    void x_SetDirtyAnnotIndex(void);
    void x_SetParentDirtyAnnotIndex(void);
    void x_ResetDirtyAnnotIndex(void);
    virtual void x_SetDirtyAnnotIndexNoParent(void);
    virtual void x_ResetDirtyAnnotIndexNoParent(void);

    void x_UpdateAnnotIndex(CTSE_Info& tse);
    virtual void x_UpdateAnnotIndexContents(CTSE_Info& tse);

    enum ENeedUpdateAux {
        /// number of bits for fields
        kNeedUpdate_bits              = 8
    };
    enum ENeedUpdate {
        /// all fields of this object
        fNeedUpdate_this              = (1<<kNeedUpdate_bits)-1,
        /// all fields of children objects
        fNeedUpdate_children          = fNeedUpdate_this<<kNeedUpdate_bits,

        /// specific fields of this object
        fNeedUpdate_descr             = 1<<0, //< descr of this object
        fNeedUpdate_annot             = 1<<1, //< annot of this object
        fNeedUpdate_seq_data          = 1<<2, //< seq-data of this object
        fNeedUpdate_core              = 1<<3, //< core
        fNeedUpdate_assembly          = 1<<4, //< assembly of this object
        fNeedUpdate_bioseq            = 1<<5, //< whole bioseq

        /// specific fields of children
        fNeedUpdate_children_descr    = fNeedUpdate_descr   <<kNeedUpdate_bits,
        fNeedUpdate_children_annot    = fNeedUpdate_annot   <<kNeedUpdate_bits,
        fNeedUpdate_children_seq_data = fNeedUpdate_seq_data<<kNeedUpdate_bits,
        fNeedUpdate_children_core     = fNeedUpdate_core    <<kNeedUpdate_bits,
        fNeedUpdate_children_assembly = fNeedUpdate_assembly<<kNeedUpdate_bits,
        fNeedUpdate_children_bioseq   = fNeedUpdate_bioseq  <<kNeedUpdate_bits
    };
    typedef int TNeedUpdateFlags;
    bool x_NeedUpdate(ENeedUpdate flag) const;
    void x_SetNeedUpdate(TNeedUpdateFlags flags);
    virtual void x_SetNeedUpdateParent(TNeedUpdateFlags flags);

    void x_Update(TNeedUpdateFlags flags) const;
    virtual void x_DoUpdate(TNeedUpdateFlags flags);

    void x_UpdateComplete(void) const;
    void x_UpdateCore(void) const;

    typedef int TChunkId;
    typedef vector<TChunkId> TChunkIds;
    void x_LoadChunk(TChunkId chunk_id) const;
    void x_LoadChunks(const TChunkIds& chunk_ids) const;

    virtual string GetDescription(void) const;

protected:
    void x_BaseParentAttach(CTSE_Info_Object& parent);
    void x_BaseParentDetach(CTSE_Info_Object& parent);
    void x_AttachObject(CTSE_Info_Object& object);
    void x_DetachObject(CTSE_Info_Object& object);


private:
    CTSE_Info_Object(const CTSE_Info_Object&);
    CTSE_Info_Object& operator=(const CTSE_Info_Object&);

    // Owner TSE info
    CTSE_Info*              m_TSE_Info;
    CTSE_Info_Object*       m_Parent_Info;
    bool                    m_DirtyAnnotIndex;
    TNeedUpdateFlags        m_NeedUpdateFlags;

    CBioObjectId            m_UniqueId;

};



/////////////////////////////////////////////////////////////////////
//
//  Inline methods
//
/////////////////////////////////////////////////////////////////////


inline
bool CTSE_Info_Object::HasTSE_Info(void) const
{
    return m_TSE_Info != 0;
}


inline
bool CTSE_Info_Object::BelongsToTSE_Info(const CTSE_Info& tse) const
{
    return m_TSE_Info == &tse;
}


inline
bool CTSE_Info_Object::HasParent_Info(void) const
{
    return m_Parent_Info != 0;
}


inline
bool CTSE_Info_Object::x_DirtyAnnotIndex(void) const
{
    return m_DirtyAnnotIndex;
}


inline
bool CTSE_Info_Object::x_NeedUpdate(ENeedUpdate flag) const
{
    return (m_NeedUpdateFlags & flag) != 0;
}

END_SCOPE(objects)
END_NCBI_SCOPE

#endif//OBJECTS_OBJMGR_IMPL___TSE_INFO_OBJECT__HPP
