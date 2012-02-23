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
*   CTSE_Info_Object
*
*/


#include <ncbi_pch.hpp>
#include <objmgr/impl/tse_info_object.hpp>
#include <objmgr/impl/tse_info.hpp>
#include <objmgr/impl/tse_chunk_info.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)


CTSE_Info_Object::CTSE_Info_Object(void)
    : m_TSE_Info(0),
      m_Parent_Info(0),
      m_DirtyAnnotIndex(true),
      m_NeedUpdateFlags(0)
{
}


CTSE_Info_Object::CTSE_Info_Object(const CTSE_Info_Object& src,
                                   TObjectCopyMap* copy_map)
    : m_TSE_Info(0),
      m_Parent_Info(0),
      m_DirtyAnnotIndex(true),
      m_NeedUpdateFlags(0)
{
    if ( copy_map ) {
        (*copy_map)[CConstRef<CObject>(&src)] = this;
    }
}


CTSE_Info_Object::~CTSE_Info_Object(void)
{
}


bool CTSE_Info_Object::HasDataSource(void) const
{
    return HasTSE_Info() && GetTSE_Info().HasDataSource();
}


const CTSE_Info& CTSE_Info_Object::GetTSE_Info(void) const
{
    _ASSERT(m_TSE_Info);
    return *m_TSE_Info;
}


CTSE_Info& CTSE_Info_Object::GetTSE_Info(void)
{
    _ASSERT(m_TSE_Info);
    return *m_TSE_Info;
}


CDataSource& CTSE_Info_Object::GetDataSource(void) const
{
    return GetTSE_Info().GetDataSource();
}


const CTSE_Info_Object& CTSE_Info_Object::GetBaseParent_Info(void) const
{
    _ASSERT(m_Parent_Info);
    return *m_Parent_Info;
}


CTSE_Info_Object& CTSE_Info_Object::GetBaseParent_Info(void)
{
    _ASSERT(m_Parent_Info);
    return *m_Parent_Info;
}


void CTSE_Info_Object::x_TSEAttach(CTSE_Info& tse)
{
    _ASSERT(!m_TSE_Info);
    _ASSERT(m_Parent_Info || &tse == this);
    x_TSEAttachContents(tse);
    _ASSERT(m_TSE_Info == &tse);
}


void CTSE_Info_Object::x_TSEDetach(CTSE_Info& tse)
{
    _ASSERT(m_TSE_Info == &tse);
    _ASSERT(m_Parent_Info || &tse == this);
    x_TSEDetachContents(tse);
    _ASSERT(!m_TSE_Info);
}


void CTSE_Info_Object::x_TSEAttachContents(CTSE_Info& tse)
{
    _ASSERT(!m_TSE_Info);
    m_TSE_Info = &tse;
    SetBioObjectId(CBioObjectId());
}


void CTSE_Info_Object::x_TSEDetachContents(CTSE_Info& _DEBUG_ARG(tse))
{
    _ASSERT(m_TSE_Info == &tse);
    m_TSE_Info->x_UnregisterBioObject(*this);
    m_TSE_Info = 0;
}


void CTSE_Info_Object::x_DSAttach(CDataSource& ds)
{
    _ASSERT(m_TSE_Info);
    _ASSERT(m_Parent_Info || m_TSE_Info == this);
    _ASSERT(!m_Parent_Info || &ds == &GetDataSource());
    x_DSAttachContents(ds);
}


void CTSE_Info_Object::x_DSDetach(CDataSource& ds)
{
    _ASSERT(m_TSE_Info);
    _ASSERT(m_Parent_Info || m_TSE_Info == this);
    _ASSERT(!m_Parent_Info || &ds == &GetDataSource());
    x_DSDetachContents(ds);
}


void CTSE_Info_Object::x_DSAttachContents(CDataSource& _DEBUG_ARG(ds))
{
    _ASSERT(&ds == &GetDataSource());
}


void CTSE_Info_Object::x_DSDetachContents(CDataSource& _DEBUG_ARG(ds))
{
    _ASSERT(&ds == &GetDataSource());
}


void CTSE_Info_Object::x_BaseParentAttach(CTSE_Info_Object& parent)
{
    _ASSERT(!m_Parent_Info);
    _ASSERT(!m_TSE_Info);
    m_Parent_Info = &parent;
    if ( x_DirtyAnnotIndex() ) {
        x_SetParentDirtyAnnotIndex();
    }
    if ( m_NeedUpdateFlags ) {
        x_SetNeedUpdateParent(m_NeedUpdateFlags);
    }
}


void CTSE_Info_Object::x_BaseParentDetach(CTSE_Info_Object& _DEBUG_ARG(parent))
{
    _ASSERT(m_Parent_Info == &parent);
    _ASSERT(!m_TSE_Info);
    m_Parent_Info = 0;
}


void CTSE_Info_Object::x_AttachObject(CTSE_Info_Object& object)
{
    _ASSERT(&object.GetBaseParent_Info() == this);
    if ( HasTSE_Info() ) {
        object.x_TSEAttach(GetTSE_Info());
    }
    if ( HasDataSource() ) {
        object.x_DSAttach(GetDataSource());
    }
}


void CTSE_Info_Object::x_DetachObject(CTSE_Info_Object& object)
{
    _ASSERT(&object.GetBaseParent_Info() == this);
    if ( HasDataSource() ) {
        object.x_DSDetach(GetDataSource());
    }
    if ( HasTSE_Info() ) {
        object.x_TSEDetach(GetTSE_Info());
    }
}


void CTSE_Info_Object::x_SetDirtyAnnotIndex(void)
{
    if ( !x_DirtyAnnotIndex() ) {
        m_DirtyAnnotIndex = true;
        x_SetParentDirtyAnnotIndex();
    }
}


void CTSE_Info_Object::x_SetParentDirtyAnnotIndex(void)
{
    if ( HasParent_Info() ) {
        GetBaseParent_Info().x_SetDirtyAnnotIndex();
    }
    else {
        x_SetDirtyAnnotIndexNoParent();
    }
}


void CTSE_Info_Object::x_SetDirtyAnnotIndexNoParent(void)
{
}


void CTSE_Info_Object::x_SetNeedUpdateParent(TNeedUpdateFlags flags)
{
    flags |= flags << kNeedUpdate_bits;
    flags &= fNeedUpdate_children;
    GetBaseParent_Info().x_SetNeedUpdate(flags);
}


void CTSE_Info_Object::x_ResetDirtyAnnotIndex(void)
{
    if ( x_DirtyAnnotIndex() ) {
        m_DirtyAnnotIndex = false;
        if ( !HasParent_Info() ) {
            x_ResetDirtyAnnotIndexNoParent();
        }
    }
}


void CTSE_Info_Object::x_ResetDirtyAnnotIndexNoParent(void)
{
}


void CTSE_Info_Object::x_UpdateAnnotIndex(CTSE_Info& tse)
{
    if ( x_DirtyAnnotIndex() ) {
        x_UpdateAnnotIndexContents(tse);
        x_ResetDirtyAnnotIndex();
    }
}


void CTSE_Info_Object::x_UpdateAnnotIndexContents(CTSE_Info& /*tse*/)
{
}


void CTSE_Info_Object::x_SetNeedUpdate(TNeedUpdateFlags flags)
{
    flags &= ~m_NeedUpdateFlags; // already set
    if ( flags ) {
        m_NeedUpdateFlags |= flags;
        if ( HasParent_Info() ) {
            x_SetNeedUpdateParent(flags);
        }
    }
}


void CTSE_Info_Object::x_Update(TNeedUpdateFlags flags) const
{
    int retry_count = 3;
    while ( m_NeedUpdateFlags & flags ) {
        if ( --retry_count < 0 ) {
            ERR_POST("CTSE_Info_Object::x_Update("<<flags<<"): "
                     "Failed to update "<<m_NeedUpdateFlags);
            break;
        }
        const_cast<CTSE_Info_Object*>(this)->
            x_DoUpdate(flags&m_NeedUpdateFlags);
    }
}


void CTSE_Info_Object::x_UpdateCore(void) const
{
    x_Update(fNeedUpdate_core|fNeedUpdate_children_core);
}


void CTSE_Info_Object::x_UpdateComplete(void) const
{
    x_Update(~0);
}


void CTSE_Info_Object::x_DoUpdate(TNeedUpdateFlags flags)
{
    m_NeedUpdateFlags &= ~flags;
}


void CTSE_Info_Object::x_LoadChunk(TChunkId chunk_id) const
{
    GetTSE_Info().x_LoadChunk(chunk_id);
}


void CTSE_Info_Object::x_LoadChunks(const TChunkIds& chunk_ids) const
{
    GetTSE_Info().x_LoadChunks(chunk_ids);
}

const CBioObjectId& CTSE_Info_Object::GetBioObjectId(void) const
{
    return m_UniqueId;
}

void CTSE_Info_Object::SetBioObjectId(const CBioObjectId& id)
{
    m_UniqueId = id;
}

END_SCOPE(objects)
END_NCBI_SCOPE
