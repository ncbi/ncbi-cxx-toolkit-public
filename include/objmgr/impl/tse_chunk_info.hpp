#ifndef OBJECTS_OBJMGR_IMPL___TSE_CHUNK_INFO__HPP
#define OBJECTS_OBJMGR_IMPL___TSE_CHUNK_INFO__HPP

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
*   Splitted TSE chunk info
*
*/


#include <corelib/ncbiobj.hpp>
#include <corelib/ncbimtx.hpp>

#include <objmgr/annot_selector.hpp>
#include <objmgr/impl/annot_object_index.hpp>

#include <vector>
#include <map>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

class CTSE_Info;
class CSeq_entry_Info;
class CSeq_annot_Info;
class CID2S_Chunk_Info;
class CID2S_Split_Info;
class CID2S_Chunk_Content;
class CID2S_Seq_annot_Info;
class CID2_Seq_loc;

class NCBI_XOBJMGR_EXPORT CTSE_Chunk_Info : public CObject
{
public:
    typedef int TChunkId;

    CTSE_Chunk_Info(CTSE_Info* tse_info, TChunkId chunk_id);
    CTSE_Chunk_Info(CTSE_Info* tse_info, const CID2S_Chunk_Info& info);
    virtual ~CTSE_Chunk_Info(void);

    CTSE_Info& GetTSE_Info(void);
    TChunkId GetChunkId(void) const;

    typedef vector< CConstRef<CID2_Seq_loc> > TLocation;
    typedef map<SAnnotTypeSelector, TLocation> TAnnotTypes;
    typedef vector<SAnnotObject_Key>  TObjectKeys;
    typedef vector<CAnnotObject_Info> TObjectInfos;

    bool NotLoaded(void) const;
    void Load(void);

    void LoadAnnotBioseq_set(int id, CSeq_annot_Info& annot_info);

    CSeq_entry_Info& GetBioseq_set(int id);
    CSeq_entry_Info& GetBioseq(int gi);

protected:
    virtual void x_Load(void);

    void x_UpdateAnnotIndex(void);
    void x_UpdateAnnotIndexThis(void);

    void x_AttachContent(const CID2S_Chunk_Content& content);
    void x_AttachAnnot(const CID2S_Seq_annot_Info& info);

private:
    friend class CTSE_Info;

    CTSE_Chunk_Info(const CTSE_Chunk_Info&);
    CTSE_Chunk_Info& operator=(const CTSE_Chunk_Info&);

    CTSE_Info*      m_TSE_Info;
    TChunkId        m_ChunkId;

    TAnnotTypes     m_AnnotTypes;
    bool            m_DirtyAnnotIndex;

    bool            m_NotLoaded;
    CFastMutex      m_LoadLock;

    TObjectKeys     m_ObjectKeys;
    TObjectInfos    m_ObjectInfos;
};


inline
CTSE_Info& CTSE_Chunk_Info::GetTSE_Info(void)
{
    return *m_TSE_Info;
}


inline
CTSE_Chunk_Info::TChunkId CTSE_Chunk_Info::GetChunkId(void) const
{
    return m_ChunkId;
}


inline
bool CTSE_Chunk_Info::NotLoaded(void) const
{
    return m_NotLoaded;
}


END_SCOPE(objects)
END_NCBI_SCOPE

/*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.1  2003/09/30 16:22:01  vasilche
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
* ===========================================================================
*/

#endif//OBJECTS_OBJMGR_IMPL___TSE_CHUNK_INFO__HPP
