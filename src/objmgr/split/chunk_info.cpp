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
* Author:  Eugene Vasilchenko
*
* File Description:
*   Application for splitting blobs withing ID1 cache
*
* ===========================================================================
*/

#include <ncbi_pch.hpp>
#include <objmgr/split/chunk_info.hpp>

#include <objmgr/split/object_splitinfo.hpp>
#include <objmgr/split/annot_piece.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)


void SChunkInfo::Add(const SChunkInfo& chunk)
{
    m_Size += chunk.m_Size;
    ITERATE ( TChunkSeq_descr, i, chunk.m_Seq_descr ) {
        TPlaceSeq_descr& dst = m_Seq_descr[i->first];
        dst.insert(dst.end(), i->second.begin(), i->second.end());
    }
    ITERATE ( TChunkAnnots, i, chunk.m_Annots ) {
        TPlaceAnnots& dst_id_annots = m_Annots[i->first];
        ITERATE ( TPlaceAnnots, j, i->second ) {
            TAnnotObjects& dst = dst_id_annots[j->first];
            dst.insert(dst.end(), j->second.begin(), j->second.end());
        }
    }
    ITERATE ( TChunkSeq_data, i, chunk.m_Seq_data ) {
        TPlaceSeq_data& dst = m_Seq_data[i->first];
        dst.insert(dst.end(), i->second.begin(), i->second.end());
    }
    ITERATE ( TChunkBioseq, i, chunk.m_Bioseq ) {
        TPlaceBioseq& dst = m_Bioseq[i->first];
        dst.insert(dst.end(), i->second.begin(), i->second.end());
    }
}


void SChunkInfo::Add(TPlaceId place_id, const CSeq_annot_SplitInfo& info)
{
    TAnnotObjects& objs = m_Annots[place_id][info.m_Src_annot];
    ITERATE ( CSeq_annot_SplitInfo::TObjects, it, info.m_Objects ) {
        if ( !*it ) {
            continue;
        }
        Add(objs, **it);
    }
}


void SChunkInfo::Add(TAnnotObjects& objs, const CLocObjects_SplitInfo& info)
{
    ITERATE ( CLocObjects_SplitInfo, it, info ) {
        objs.push_back(*it);
        m_Size += it->m_Size;
    }
}


void SChunkInfo::Add(const SAnnotPiece& piece)
{
    switch ( piece.m_ObjectType ) {
    case SAnnotPiece::seq_descr:
        Add(piece.m_PlaceId, *piece.m_Seq_descr);
        break;
    case SAnnotPiece::annot_object:
    {{
        TPlaceAnnots& place_annots = m_Annots[piece.m_PlaceId];
        TAnnotObjects& objs = place_annots[piece.m_Seq_annot->m_Src_annot];
        objs.push_back(*piece.m_AnnotObject);
        m_Size += piece.m_Size;
        break;
    }}
    case SAnnotPiece::seq_annot:
        Add(piece.m_PlaceId, *piece.m_Seq_annot);
        break;
    case SAnnotPiece::seq_data:
        Add(piece.m_PlaceId, *piece.m_Seq_data);
        break;
    case SAnnotPiece::bioseq:
        Add(piece.m_PlaceId, *piece.m_Bioseq);
        break;
    default:
        _ASSERT(0 && "unknown annot type");
    }
}


void SChunkInfo::Add(const SIdAnnotPieces& pieces)
{
    ITERATE ( SIdAnnotPieces, it, pieces ) {
        Add(*it);
    }
}


void SChunkInfo::Add(TPlaceId place_id, const CSeq_inst_SplitInfo& info)
{
    ITERATE ( CSeq_inst_SplitInfo::TSeq_data, it, info.m_Seq_data ) {
        Add(place_id, *it);
    }
}


void SChunkInfo::Add(TPlaceId place_id, const CSeq_data_SplitInfo& info)
{
    m_Seq_data[place_id].push_back(info);
    m_Size += info.m_Size;
}


void SChunkInfo::Add(TPlaceId place_id, const CSeq_descr_SplitInfo& info)
{
    m_Seq_descr[place_id].push_back(info);
    m_Size += info.m_Size;
}


void SChunkInfo::Add(TPlaceId place_id, const CBioseq_SplitInfo& info)
{
    m_Bioseq[place_id].push_back(info);
    m_Size += info.m_Size;
}


size_t SChunkInfo::CountAnnotObjects(void) const
{
    size_t count = 0;
    ITERATE ( TChunkAnnots, i, m_Annots ) {
        ITERATE ( TPlaceAnnots, j, i->second ) {
            count += j->second.size();
        }
    }
    return count;
}


END_SCOPE(objects)
END_NCBI_SCOPE

/*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.9  2004/08/24 16:45:15  vasilche
* Removed excessive _TRACEs.
*
* Revision 1.8  2004/08/19 14:18:54  vasilche
* Added splitting of whole Bioseqs.
*
* Revision 1.7  2004/08/04 14:48:21  vasilche
* Added joining of very small chunks with skeleton.
*
* Revision 1.6  2004/06/30 20:56:32  vasilche
* Added splitting of Seqdesr objects (disabled yet).
*
* Revision 1.5  2004/06/15 14:05:50  vasilche
* Added splitting of sequence.
*
* Revision 1.4  2004/05/21 21:42:14  gorelenk
* Added PCH ncbi_pch.hpp
*
* Revision 1.3  2004/01/07 17:36:25  vasilche
* Moved id2_split headers to include/objmgr/split.
* Fixed include path to genbank.
*
* Revision 1.2  2003/11/26 23:04:58  vasilche
* Removed extra semicolons after BEGIN_SCOPE and END_SCOPE.
*
* Revision 1.1  2003/11/12 16:18:29  vasilche
* First implementation of ID2 blob splitter withing cache.
*
* ===========================================================================
*/
