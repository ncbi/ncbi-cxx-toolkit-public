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
#include <list>
#include <map>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

class CTSE_Info;
class CSeq_entry_Info;
class CSeq_annot_Info;
class CSeq_literal;
class CBioseq_Base_Info;
class CBioseq_Info;

class NCBI_XOBJMGR_EXPORT CTSE_Chunk_Info : public CObject
{
public:
    // chunk identification
    typedef int TChunkId;

    // contents place identification
    enum EPlaceType {
        eBioseq,
        eBioseq_set
    };
    typedef int TPlaceId;
    typedef pair<EPlaceType, TPlaceId> TPlace;
    typedef vector<TPlace> TPlaces;

    // annot contents identification
    typedef CSeq_id_Handle TLocationId;
    typedef CRange<TSeqPos> TLocationRange;
    typedef pair<TLocationId, TLocationRange> TLocation;
    typedef vector<TLocation> TLocationSet;
    typedef map<SAnnotTypeSelector, TLocationSet> TAnnotTypes;
    typedef map<CAnnotName, TAnnotTypes> TAnnotContents;

    // annot contents indexing
    typedef SAnnotObjects_Info TObjectInfos;
    typedef list<TObjectInfos> TObjectInfosList;

    // constructors
    CTSE_Chunk_Info(TChunkId id);
    virtual ~CTSE_Chunk_Info(void);

    CTSE_Info& GetTSE_Info(void);

    TChunkId GetChunkId(void) const;

    bool NotLoaded(void) const;
    void Load(void);

    void x_TSEAttach(CTSE_Info& tse_info);

    void x_AddAnnotPlace(EPlaceType place_type, TPlaceId place_id);
    CBioseq_Base_Info& x_GetBase(const TPlace& place);
    CBioseq_Info& x_GetBioseq(const TPlace& place);

    void x_AddAnnotType(const CAnnotName& annot_name,
                        const SAnnotTypeSelector& annot_type,
                        const TLocationId& location_id,
                        const TLocationRange& location_range);
    void x_AddAnnotType(const CAnnotName& annot_name,
                        const SAnnotTypeSelector& annot_type,
                        const TLocationSet& location);
    void x_AddSeq_data(const TLocationSet& location);

    void x_LoadAnnot(const TPlace& place, CRef<CSeq_annot_Info> annot);

    typedef list< CRef<CSeq_literal> > TSequence;
    void x_LoadSequence(const TPlace& place, TSeqPos pos,
                        const TSequence& seq);

protected:
    virtual void x_Load(void);

    void x_UpdateAnnotIndex(CTSE_Info& tse);
    void x_UpdateAnnotIndexContents(CTSE_Info& tse);
    void x_UnmapAnnotObjects(CTSE_Info& tse);
    void x_DropAnnotObjects(CTSE_Info& tse);

    void x_TSEAttachSeq_data(void);

private:
    friend class CTSE_Info;

    CTSE_Chunk_Info(const CTSE_Chunk_Info&);
    CTSE_Chunk_Info& operator=(const CTSE_Chunk_Info&);

    CTSE_Info*      m_TSE_Info;
    TChunkId        m_ChunkId;

    bool            m_DirtyAnnotIndex;
    bool            m_NotLoaded;

    TPlaces         m_AnnotPlaces;
    TAnnotContents  m_AnnotContents;
    TLocationSet    m_Seq_data;

    CFastMutex      m_LoadLock;

    TObjectInfosList  m_ObjectInfosList;
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
* Revision 1.6  2004/06/15 14:06:49  vasilche
* Added support to load split sequences.
*
* Revision 1.5  2004/03/16 15:47:27  vasilche
* Added CBioseq_set_Handle and set of EditHandles
*
* Revision 1.4  2004/01/22 20:10:39  vasilche
* 1. Splitted ID2 specs to two parts.
* ID2 now specifies only protocol.
* Specification of ID2 split data is moved to seqsplit ASN module.
* For now they are still reside in one resulting library as before - libid2.
* As the result split specific headers are now in objects/seqsplit.
* 2. Moved ID2 and ID1 specific code out of object manager.
* Protocol is processed by corresponding readers.
* ID2 split parsing is processed by ncbi_xreader library - used by all readers.
* 3. Updated OBJMGR_LIBS correspondingly.
*
* Revision 1.3  2003/11/26 17:55:55  vasilche
* Implemented ID2 split in ID1 cache.
* Fixed loading of splitted annotations.
*
* Revision 1.2  2003/10/07 13:43:22  vasilche
* Added proper handling of named Seq-annots.
* Added feature search from named Seq-annots.
* Added configurable adaptive annotation search (default: gene, cds, mrna).
* Fixed selection of blobs for loading from GenBank.
* Added debug checks to CSeq_id_Mapper for easier finding lost CSeq_id_Handles.
* Fixed leaked split chunks annotation stubs.
* Moved some classes definitions in separate *.cpp files.
*
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
