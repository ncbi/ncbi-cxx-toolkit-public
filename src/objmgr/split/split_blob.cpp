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

#include "splitted_blob.hpp"

#include <objects/seqset/Seq_entry.hpp>

#include <objects/id2/ID2S_Split_Info.hpp>
#include <objects/id2/ID2S_Chunk.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

/////////////////////////////////////////////////////////////////////////////
// CSplittedBlob
/////////////////////////////////////////////////////////////////////////////

CSplittedBlob::CSplittedBlob(void)
{
}


CSplittedBlob::~CSplittedBlob(void)
{
}


CSplittedBlob::CSplittedBlob(const CSplittedBlob& blob)
    : m_MainBlob(blob.m_MainBlob),
      m_SplitInfo(blob.m_SplitInfo),
      m_Chunks(blob.m_Chunks)
{
}


CSplittedBlob& CSplittedBlob::operator=(const CSplittedBlob& blob)
{
    m_MainBlob = blob.m_MainBlob;
    m_SplitInfo = blob.m_SplitInfo;
    m_Chunks = blob.m_Chunks;
    return *this;
}


void CSplittedBlob::Reset(void)
{
    m_MainBlob.Reset();
    m_SplitInfo.Reset();
    m_Chunks.clear();
}


void CSplittedBlob::Reset(const CSeq_entry& entry)
{
    Reset();
    m_MainBlob.Reset(&entry);
}


void CSplittedBlob::Reset(const CSeq_entry& skeleton,
                          const CID2S_Split_Info& split_info)
{
    Reset();
    m_MainBlob.Reset(&skeleton);
    m_SplitInfo.Reset(&split_info);
}


void CSplittedBlob::AddChunk(const CID2S_Chunk_Id& id,
                             const CID2S_Chunk& chunk)
{
    m_Chunks[id].Reset(&chunk);
}


END_SCOPE(objects)
END_NCBI_SCOPE

/*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.3  2003/12/02 19:12:24  vasilche
* Fixed compilation on MSVC.
*
* Revision 1.2  2003/11/26 23:05:00  vasilche
* Removed extra semicolons after BEGIN_SCOPE and END_SCOPE.
*
* Revision 1.1  2003/11/12 16:18:33  vasilche
* First implementation of ID2 blob splitter withing cache.
*
* ===========================================================================
*/
