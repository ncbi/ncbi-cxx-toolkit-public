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


void SChunkInfo::Add(const CSeq_annot_SplitInfo& info)
{
    _TRACE("SChunkInfo::Add(const CSeq_annot_SplitInfo&)");
    TAnnotObjects& objs = m_Annots[info.m_Id][info.m_Src_annot];
    Add(objs, info.m_LandmarkObjects);
    Add(objs, info.m_ComplexLocObjects);
    ITERATE ( TSimpleLocObjects, it, info.m_SimpleLocObjects ) {
        Add(objs, it->second);
    }
}


void SChunkInfo::Add(TAnnotObjects& objs, const CLocObjects_SplitInfo& info)
{
    _TRACE("SChunkInfo::Add(TAnnotObjects& objst, CLocObjects_SplitInfo&)");
    ITERATE ( CLocObjects_SplitInfo, it, info ) {
        objs.push_back(*it);
        m_Size += it->m_Size;
    }
}


void SChunkInfo::Add(const SAnnotPiece& piece)
{
    _TRACE("SChunkInfo::Add(const SAnnotPiece&)");
    const CSeq_annot_SplitInfo& info = *piece.m_Seq_annot;
    if ( piece.m_Object ) {
        TAnnotObjects& objs = m_Annots[info.m_Id][info.m_Src_annot];
        objs.push_back(*piece.m_Object);
        m_Size += piece.m_Size;
    }
    else {
        Add(info);
    }
}


void SChunkInfo::Add(const SIdAnnotPieces& pieces)
{
    _TRACE("SChunkInfo::Add(const SIdAnnotPieces&)");
    ITERATE ( SIdAnnotPieces, it, pieces ) {
        Add(*it);
    }
}


size_t SChunkInfo::CountAnnotObjects(void) const
{
    size_t count = 0;
    ITERATE ( TChunkAnnots, i, m_Annots ) {
        ITERATE ( TIdAnnots, j, i->second ) {
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
