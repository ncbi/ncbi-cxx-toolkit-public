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

#include "blob_splitter_params.hpp"

#include <util/compress/zlib.hpp>

BEGIN_NCBI_SCOPE;
BEGIN_SCOPE(objects);


void SSplitterParams::Compress(vector<char>& dst,
                               const char* data, size_t size) const
{
    switch ( m_Compression ) {
    case eCompression_none:
        dst.resize(size);
        memcpy(&dst[0], data, size);
        break;
    case eCompression_nlm_zip:
    {{
        CZipCompression compr(CCompression::eLevel_Default);
        dst.resize(size_t(size*1.01) + 32);
        size_t real_size = 0;
        if ( !compr.CompressBuffer(data, size,
                                   &dst[8], dst.size(), &real_size) ) {
            NCBI_THROW(CLoaderException, eCompressionError,
                "zip compression failed");
        }
        for ( size_t i = 0, s = real_size; i < 4; ++i, s <<= 8 ) {
            dst[i] = (s >> 24) & 0xff;
        }
        for ( size_t i = 0, s = size; i < 4; ++i, s <<= 8 ) {
            dst[i+4] = (s >> 24) & 0xff;
        }
        dst.resize(8+real_size);
        break;
    }}
    default:
        NCBI_THROW(CLoaderException, eCompressionError,
            "compression method is not implemented");
    }
}


END_SCOPE(objects);
END_NCBI_SCOPE;

/*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.2  2003/11/19 22:18:05  grichenk
* All exceptions are now CException-derived. Catch "exception" rather
* than "runtime_error".
*
* Revision 1.1  2003/11/12 16:18:28  vasilche
* First implementation of ID2 blob splitter withing cache.
*
* ===========================================================================
*/
