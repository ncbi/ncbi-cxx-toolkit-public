#ifndef NCBI_OBJMGR_SPLIT_ID2_COMPRESS__HPP
#define NCBI_OBJMGR_SPLIT_ID2_COMPRESS__HPP

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

#include <vector>
#include <list>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

struct SSplitterParams;

class NCBI_ID2_SPLIT_EXPORT CId2Compressor
{
public:
    static void Compress(const SSplitterParams& params,
                         list<vector<char>*>& dst,
                         const char* data, size_t size);
    static void Compress(const SSplitterParams& params,
                         vector<char>& dst,
                         const char* data, size_t size);

    static void CompressHeader(const SSplitterParams& params,
                               vector<char>& dst,
                               size_t size);
    static void CompressFooter(const SSplitterParams& params,
                               vector<char>& dst,
                               size_t size);
    static void CompressChunk(const SSplitterParams& params,
                              vector<char>& dst,
                              const char* data, size_t size);
    static void sx_Append(vector<char>& dst,
                          const char* data, size_t size);
};


END_SCOPE(objects)
END_NCBI_SCOPE

/*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.2  2004/08/04 14:48:49  vasilche
* Added exports for MSVC. Added joining of very small chunks with skeleton.
*
* Revision 1.1  2003/12/30 16:06:14  vasilche
* Compression methods moved to separate header: id2_compress.hpp.
*
*
* ===========================================================================
*/
#endif//NCBI_OBJMGR_SPLIT_ID2_COMPRESS__HPP
