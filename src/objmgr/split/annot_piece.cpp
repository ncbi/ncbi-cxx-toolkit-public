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
    : m_Object(0),
      m_Seq_annot(0)
{
}


SAnnotPiece::SAnnotPiece(const SAnnotPiece& piece,
                         const COneSeqRange& range)
    : m_Size(piece.m_Size),
      m_Location(piece.m_Location),
      m_IdRange(range.GetTotalRange()),
      m_Object(piece.m_Object),
      m_Seq_annot(piece.m_Seq_annot)
{
}


SAnnotPiece::SAnnotPiece(const CAnnotObject_SplitInfo& obj,
                         const CSeq_annot_SplitInfo& annot)
    : m_Size(obj.m_Size),
      m_Location(obj.m_Location),
      m_IdRange(TRange::GetEmpty()),
      m_Object(&obj),
      m_Seq_annot(&annot)
{
}


SAnnotPiece::SAnnotPiece(const CSeq_annot_SplitInfo& annot)
    : m_Size(annot.m_ComplexLocObjects.m_Size),
      m_Location(annot.m_ComplexLocObjects.m_Location),
      m_IdRange(TRange::GetEmpty()),
      m_Object(0),
      m_Seq_annot(&annot)
{
    ITERATE ( TSimpleLocObjects, it, annot.m_SimpleLocObjects ) {
        m_Size += it->second.m_Size;
        m_Location.Add(it->second.m_Location);
    }
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


void CAnnotPieces::Add(const CBioseq_SplitInfo& bioseq_info,
                       SChunkInfo& main_chunk)
{
    ITERATE ( CBioseq_SplitInfo::TSeq_annots, it, bioseq_info.m_Seq_annots ) {
        Add(it->second, main_chunk);
    }
}


void CAnnotPieces::Add(const CSeq_annot_SplitInfo& info,
                       SChunkInfo& main_chunk)
{
    size_t max_size = info.m_Name.IsNamed()? 100: 10;
    size_t size = info.m_ComplexLocObjects.size();
    ITERATE ( TSimpleLocObjects, idit, info.m_SimpleLocObjects ) {
        size += idit->second.size();
    }
    if ( size <= max_size ) {
        if ( !info.m_LandmarkObjects.empty() ) {
            main_chunk.Add(info);
        }
        else {
            Add(SAnnotPiece(info));
        }
    }
    else {
        if ( !info.m_LandmarkObjects.empty() ) {
            main_chunk.Add(CSeq_annot_SplitInfo(info, info.m_LandmarkObjects));
        }
        Add(info.m_ComplexLocObjects, info);
        ITERATE ( TSimpleLocObjects, idit, info.m_SimpleLocObjects ) {
            Add(idit->second, info);
        }
    }
}


void CAnnotPieces::Add(const CLocObjects_SplitInfo& objs,
                       const CSeq_annot_SplitInfo& annot)
{
    ITERATE ( CLocObjects_SplitInfo, it, objs ) {
        Add(SAnnotPiece(*it, annot));
    }
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
            if ( p.m_Object ) {
                cnt = 1;
            }
            else {
                cnt = CSeq_annot_SplitInfo::
                    CountAnnotObjects(*p.m_Seq_annot->m_Src_annot);
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
