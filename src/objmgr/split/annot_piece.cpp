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
#include <objmgr/split/annot_piece.hpp>

#include <objmgr/split/object_splitinfo.hpp>
#include <objmgr/split/chunk_info.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

SAnnotPiece::SAnnotPiece(void)
    : m_PlaceId(0),
      m_ObjectType(empty),
      m_AnnotObject(0)
{
    m_Object = 0;
}


SAnnotPiece::SAnnotPiece(const SAnnotPiece& piece,
                         const COneSeqRange& range)
    : m_PlaceId(piece.m_PlaceId),
      m_ObjectType(piece.m_ObjectType),
      m_AnnotObject(piece.m_AnnotObject),
      m_Priority(piece.m_Priority),
      m_Size(piece.m_Size),
      m_Location(piece.m_Location),
      m_IdRange(range.GetTotalRange())
{
    m_Object = piece.m_Object;
}


SAnnotPiece::SAnnotPiece(TPlaceId place_id,
                         const CSeq_annot_SplitInfo& annot,
                         const CAnnotObject_SplitInfo& obj)
    : m_PlaceId(place_id),
      m_ObjectType(annot_object),
      m_AnnotObject(&obj),
      m_Priority(obj.GetPriority()),
      m_Size(obj.m_Size),
      m_Location(obj.m_Location),
      m_IdRange(TRange::GetEmpty())
{
    m_Seq_annot = &annot;
}


SAnnotPiece::SAnnotPiece(TPlaceId place_id, const CSeq_annot_SplitInfo& annot)
    : m_PlaceId(place_id),
      m_ObjectType(seq_annot),
      m_AnnotObject(0),
      m_Priority(annot.GetPriority()),
      m_Size(annot.m_Size),
      m_Location(annot.m_Location),
      m_IdRange(TRange::GetEmpty())
{
    m_Seq_annot = &annot;
}


SAnnotPiece::SAnnotPiece(TPlaceId place_id, const CSeq_data_SplitInfo& data)
    : m_PlaceId(place_id),
      m_ObjectType(seq_data),
      m_AnnotObject(0),
      m_Priority(data.GetPriority()),
      m_Size(data.m_Size),
      m_Location(data.m_Location),
      m_IdRange(TRange::GetEmpty())
{
    m_Seq_data = &data;
}


SAnnotPiece::SAnnotPiece(TPlaceId place_id, const CSeq_descr_SplitInfo& descr)
    : m_PlaceId(place_id),
      m_ObjectType(seq_descr),
      m_AnnotObject(0),
      m_Priority(descr.GetPriority()),
      m_Size(descr.m_Size),
      m_Location(descr.m_Location),
      m_IdRange(TRange::GetEmpty())
{
    m_Seq_descr = &descr;
}


SAnnotPiece::SAnnotPiece(TPlaceId place_id, const CBioseq_SplitInfo& seq)
    : m_PlaceId(place_id),
      m_ObjectType(bioseq),
      m_AnnotObject(0),
      m_Priority(seq.GetPriority()),
      m_Size(seq.m_Size),
      m_Location(seq.m_Location),
      m_IdRange(TRange::GetEmpty())
{
    m_Bioseq = &seq;
}


bool SAnnotPiece::operator<(const SAnnotPiece& piece) const
{
    if ( m_IdRange != piece.m_IdRange ) {
        return m_IdRange < piece.m_IdRange;
    }
    if ( m_ObjectType != piece.m_ObjectType ) {
        return m_ObjectType < piece.m_ObjectType;
    }
    if ( m_Object != piece.m_Object ) {
        return m_Object < piece.m_Object;
    }
    if ( m_AnnotObject != piece.m_AnnotObject ) {
        return m_AnnotObject < piece.m_AnnotObject;
    }
    return false;
}


bool SAnnotPiece::operator==(const SAnnotPiece& piece) const
{
    return
        m_IdRange == piece.m_IdRange &&
        m_ObjectType == piece.m_ObjectType &&
        m_Object == piece.m_Object &&
        m_AnnotObject == piece.m_AnnotObject;
}


bool SAnnotPiece::operator!=(const SAnnotPiece& piece) const
{
    return
        m_IdRange != piece.m_IdRange ||
        m_ObjectType != piece.m_ObjectType ||
        m_Object != piece.m_Object ||
        m_AnnotObject != piece.m_AnnotObject;
}


void SIdAnnotPieces::Add(const SAnnotPiece& piece)
{
    m_Pieces.insert(piece);
    m_Size += piece.m_Size;
    m_IdRange += piece.m_IdRange;
}


void SIdAnnotPieces::Remove(const SAnnotPiece& piece)
{
    m_Size -= piece.m_Size;
    _VERIFY(m_Pieces.erase(piece) == 1);
}


SIdAnnotPieces::TPieces::iterator SIdAnnotPieces::Erase(TPieces::iterator it)
{
    m_Size -= it->m_Size;
    TPieces::iterator erase = it++;
    m_Pieces.erase(erase);
    return it;
}


/////////////////////////////////////////////////////////////////////////////
// CAnnotPieces
/////////////////////////////////////////////////////////////////////////////


CAnnotPieces::CAnnotPieces(void)
{
}


CAnnotPieces::~CAnnotPieces(void)
{
}


void CAnnotPieces::Add(const SAnnotPiece& piece)
{
    ITERATE ( CSeqsRange, it, piece.m_Location ) {
        SIdAnnotPieces& info = m_PiecesById[it->first];
        info.Add(SAnnotPiece(piece, it->second));
    }
}


void CAnnotPieces::Remove(const SAnnotPiece& piece)
{
    ITERATE ( CSeqsRange, it, piece.m_Location ) {
        SIdAnnotPieces& info = m_PiecesById[it->first];
        info.Remove(SAnnotPiece(piece, it->second));
    }
}


size_t CAnnotPieces::CountAnnotObjects(void) const
{
    double ref_count = 0;
    ITERATE ( TPiecesById, i, m_PiecesById ) {
        const SIdAnnotPieces& pp = i->second;
        ITERATE ( SIdAnnotPieces, j, pp ) {
            const SAnnotPiece& p = *j;
            size_t cnt;
            switch ( p.m_ObjectType ) {
            case SAnnotPiece::empty:
                cnt = 0;
                break;
            case SAnnotPiece::seq_annot:
                cnt = CSeq_annot_SplitInfo::
                    CountAnnotObjects(*p.m_Seq_annot->m_Src_annot);
                break;
            default:
                cnt = 1;
            }
            size_t id_refs = p.m_Location.size();
            ref_count += double(cnt) / id_refs;
        }
    }
    return size_t(ref_count + .5);
}


END_SCOPE(objects)
END_NCBI_SCOPE

/*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.7  2004/08/19 14:18:54  vasilche
* Added splitting of whole Bioseqs.
*
* Revision 1.6  2004/06/30 20:56:32  vasilche
* Added splitting of Seqdesr objects (disabled yet).
*
* Revision 1.5  2004/06/15 14:05:50  vasilche
* Added splitting of sequence.
*
* Revision 1.4  2004/05/21 21:42:13  gorelenk
* Added PCH ncbi_pch.hpp
*
* Revision 1.3  2004/01/07 17:36:21  vasilche
* Moved id2_split headers to include/objmgr/split.
* Fixed include path to genbank.
*
* Revision 1.2  2003/11/26 23:04:56  vasilche
* Removed extra semicolons after BEGIN_SCOPE and END_SCOPE.
*
* Revision 1.1  2003/11/12 16:18:23  vasilche
* First implementation of ID2 blob splitter withing cache.
*
* ===========================================================================
*/
