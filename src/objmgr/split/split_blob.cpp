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
#include <objmgr/split/split_blob.hpp>

#include <objects/seqset/Seq_entry.hpp>

#include <objects/seqsplit/ID2S_Split_Info.hpp>
#include <objects/seqsplit/ID2S_Chunk.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

/////////////////////////////////////////////////////////////////////////////
// CSplitBlob
/////////////////////////////////////////////////////////////////////////////

CSplitBlob::CSplitBlob(void)
{
}


CSplitBlob::~CSplitBlob(void)
{
}


CSplitBlob::CSplitBlob(const CSplitBlob& blob)
    : m_MainBlob(blob.m_MainBlob),
      m_SplitInfo(blob.m_SplitInfo),
      m_Chunks(blob.m_Chunks)
{
}


CSplitBlob& CSplitBlob::operator=(const CSplitBlob& blob)
{
    m_MainBlob = blob.m_MainBlob;
    m_SplitInfo = blob.m_SplitInfo;
    m_Chunks = blob.m_Chunks;
    return *this;
}


void CSplitBlob::Reset(void)
{
    m_MainBlob.Reset();
    m_SplitInfo.Reset();
    m_Chunks.clear();
}


void CSplitBlob::Reset(const CSeq_entry& entry)
{
    Reset();
    m_MainBlob.Reset(&entry);
}


void CSplitBlob::Reset(const CSeq_entry& skeleton,
                          const CID2S_Split_Info& split_info)
{
    Reset();
    m_MainBlob.Reset(&skeleton);
    m_SplitInfo.Reset(&split_info);
}


void CSplitBlob::AddChunk(const CID2S_Chunk_Id& id,
                             const CID2S_Chunk& chunk)
{
    m_Chunks[id].Reset(&chunk);
}


END_SCOPE(objects)
END_NCBI_SCOPE

/*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.8  2004/05/21 21:42:14  gorelenk
* Added PCH ncbi_pch.hpp
*
* Revision 1.7  2004/01/22 20:10:42  vasilche
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
* Revision 1.6  2004/01/07 17:36:29  vasilche
* Moved id2_split headers to include/objmgr/split.
* Fixed include path to genbank.
*
* Revision 1.5  2003/12/03 19:40:57  kuznets
* Minor file rename
*
* Revision 1.4  2003/12/03 19:30:45  kuznets
* Misprint fixed
*
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
