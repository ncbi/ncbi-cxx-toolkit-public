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
        kDefaultChunkSize = 20 * 1024
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
    ECompression m_Compression;
    TVerbose     m_Verbose;

    bool         m_DisableSplitDescriptions;
    bool         m_DisableSplitSequence;
    bool         m_DisableSplitAnnotations;
    bool         m_JoinSmallChunks;
};


END_SCOPE(objects)
END_NCBI_SCOPE

/*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.8  2004/08/04 14:48:49  vasilche
* Added exports for MSVC. Added joining of very small chunks with skeleton.
*
* Revision 1.7  2004/06/15 14:05:49  vasilche
* Added splitting of sequence.
*
* Revision 1.6  2004/03/05 17:40:34  vasilche
* Added 'verbose' option to splitter parameters.
*
* Revision 1.5  2003/12/30 16:06:14  vasilche
* Compression methods moved to separate header: id2_compress.hpp.
*
* Revision 1.4  2003/12/18 21:15:35  vasilche
* Fixed long -> double conversion warning.
*
* Revision 1.3  2003/11/26 23:04:58  vasilche
* Removed extra semicolons after BEGIN_SCOPE and END_SCOPE.
*
* Revision 1.2  2003/11/26 17:56:02  vasilche
* Implemented ID2 split in ID1 cache.
* Fixed loading of splitted annotations.
*
* Revision 1.1  2003/11/12 16:18:29  vasilche
* First implementation of ID2 blob splitter withing cache.
*
* ===========================================================================
*/
#endif//NCBI_OBJMGR_SPLIT_BLOB_SPLITTER_PARAMS__HPP
