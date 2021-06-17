#ifndef NCBI_OBJMGR_SPLIT_BLOB_SPLITTER_PARAMS__HPP
#define NCBI_OBJMGR_SPLIT_BLOB_SPLITTER_PARAMS__HPP

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

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

struct NCBI_ID2_SPLIT_EXPORT SSplitterParams
{
    SSplitterParams(void);

    enum {
        kDefaultChunkSize = 20 * 1024,
        kDefaultMinChunkCount = 1,
        kDefaultSplitNonFeatureSeqTables = 1
    };

    void SetChunkSize(size_t size);

    enum ECompression
    {
        eCompression_none,
        eCompression_nlm_zip,
        eCompression_gzip
    };

    typedef int TVerbose;

    // parameters
    size_t       m_ChunkSize;
    size_t       m_MinChunkSize;
    size_t       m_MaxChunkSize;
    size_t       m_MinChunkCount;
    ECompression m_Compression;
    TVerbose     m_Verbose;

    bool         m_DisableSplitDescriptions;
    bool         m_DisableSplitSequence;
    bool         m_DisableSplitAnnotations;
    bool         m_DisableSplitAssembly;
    bool         m_JoinSmallChunks;
    bool         m_SplitWholeBioseqs;
    bool         m_SplitNonFeatureSeqTables;
};


END_SCOPE(objects)
END_NCBI_SCOPE

#endif//NCBI_OBJMGR_SPLIT_BLOB_SPLITTER_PARAMS__HPP
