#ifndef NCBI_OBJMGR_SPLIT_CHUNK_INFO__HPP
#define NCBI_OBJMGR_SPLIT_CHUNK_INFO__HPP

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

#include <corelib/ncbistd.hpp>
#include <corelib/ncbiobj.hpp>

#include <memory>
#include <map>
#include <vector>

#include "size.hpp"

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

class CSeq_annot;

class CAnnotObject_SplitInfo;
class CLocObjects_SplitInfo;
class CSeq_annot_SplitInfo;
class CBioseq_SplitInfo;

struct SAnnotPiece;
struct SIdAnnotPieces;
struct SAnnotPieces;

struct SChunkInfo
{
    typedef vector<CAnnotObject_SplitInfo> TAnnotObjects;
    typedef map<CConstRef<CSeq_annot>, TAnnotObjects> TIdAnnots;
    typedef map<int, TIdAnnots> TChunkAnnots;

    void Add(const CSeq_annot_SplitInfo& info);
    void Add(TAnnotObjects& objs,
             const CLocObjects_SplitInfo& info);
    void Add(const SAnnotPiece& piece);
    void Add(const SIdAnnotPieces& pieces);

    size_t CountAnnotObjects(void) const;

    CSize m_Size;
    TChunkAnnots m_Annots;
};


END_SCOPE(objects)
END_NCBI_SCOPE

/*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.2  2003/11/26 23:04:59  vasilche
* Removed extra semicolons after BEGIN_SCOPE and END_SCOPE.
*
* Revision 1.1  2003/11/12 16:18:30  vasilche
* First implementation of ID2 blob splitter withing cache.
*
* ===========================================================================
*/
#endif//NCBI_OBJMGR_SPLIT_CHUNK_INFO__HPP
