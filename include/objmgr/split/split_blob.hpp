#ifndef NCBI_OBJMGR_SPLIT_SPLITTED_BLOB__HPP
#define NCBI_OBJMGR_SPLIT_SPLITTED_BLOB__HPP

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
#include <objects/seqsplit/ID2S_Chunk_Id.hpp>
#include <vector>
#include <map>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

class CSeq_entry;
class CID2S_Split_Info;
class CID2S_Chunk;
class CID2S_Chunk_Id;

class NCBI_ID2_SPLIT_EXPORT CSplitBlob
{
public:
    CSplitBlob(void);
    ~CSplitBlob(void);

    CSplitBlob(const CSplitBlob& blob);
    CSplitBlob& operator=(const CSplitBlob& blob);

    void Reset(void);
    void Reset(const CSeq_entry& entry);
    void Reset(const CSeq_entry& skeleton,
               const CID2S_Split_Info& split_info);
    void AddChunk(const CID2S_Chunk_Id& id, const CID2S_Chunk& chunk);

    bool IsSplit(void) const
        {
            return m_SplitInfo.NotEmpty();
        }

    typedef map<CID2S_Chunk_Id, CConstRef<CID2S_Chunk> > TChunks;

    const CSeq_entry& GetMainBlob(void) const
        {
            return *m_MainBlob;
        }
    const CID2S_Split_Info& GetSplitInfo(void) const
        {
            return *m_SplitInfo;
        }
    const TChunks& GetChunks(void) const
        {
            return m_Chunks;
        }

private:

    CConstRef<CSeq_entry> m_MainBlob;
    CConstRef<CID2S_Split_Info> m_SplitInfo;
    TChunks m_Chunks;
};


END_SCOPE(objects)
END_NCBI_SCOPE

#endif//NCBI_OBJMGR_SPLIT_SPLITTED_BLOB__HPP
