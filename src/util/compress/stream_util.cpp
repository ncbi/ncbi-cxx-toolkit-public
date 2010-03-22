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
 * Authors:  Vladimir Ivanov
 *
 */

#include <ncbi_pch.hpp>
#include <util/compress/stream_util.hpp>


BEGIN_NCBI_SCOPE


// Algorithm-specific defaults
const ICompression::TFlags kDefault_BZip2    = 0;
const ICompression::TFlags kDefault_LZO      = 0;
const ICompression::TFlags kDefault_Zip      = 0;
const ICompression::TFlags kDefault_GZipFile = CZipCompression::fCheckFileHeader | CZipCompression::fWriteGZipFormat;
const ICompression::TFlags kDefault_ConcatenatedGZipFile = CZipCompression::fGZip;


CCompressionStreamProcessor* CCompressStream::x_Init(EInitType            type,
                                                     EMethod              method, 
                                                     ICompression::TFlags flags)
{
    CCompressionStreamProcessor* processor = 0;

    switch(method) {
    case eBZip2:
        if (flags = fDefault) {
            flags = kDefault_BZip2;
        } else {
            flags |= kDefault_BZip2;
        }
        if (type == eCompress) {
            processor = new CBZip2StreamCompressor(ICompression::eLevel_Default, flags);
        } else {
            processor = new CBZip2StreamDecompressor(flags);
        }
        break;

    case eLZO:
#if defined(HAVE_LIBLZO)
        if (flags = fDefault) {
            flags = kDefault_LZO;
        } else {
            flags |= kDefault_LZO;
        }
        if (type == eCompress) {
            processor = new CLZOStreamCompressor(ICompression::eLevel_Default, flags);
        } else {
            processor = new CLZOStreamDecompressor(flags);
        }
#endif 
        break;

    case eZip:
        if (flags = fDefault) {
            flags = kDefault_Zip;
        } else {
            flags |= kDefault_Zip;
        }
        if (type == eCompress) {
            processor = new CZipStreamCompressor(ICompression::eLevel_Default, flags);
        } else {
            processor = new CZipStreamDecompressor(flags);
        }
        break;

    case eGZipFile:
        if (flags = fDefault) {
            flags = kDefault_GZipFile;
        } else {
            flags |= kDefault_GZipFile;
        }
        if (type == eCompress) {
            processor = new CZipStreamCompressor(ICompression::eLevel_Default, flags);
        } else {
            processor = new CZipStreamDecompressor(flags);
        }
        break;

    case eConcatenatedGZipFile:
        if (flags = fDefault) {
            flags = kDefault_ConcatenatedGZipFile;
        } else {
            flags |= kDefault_ConcatenatedGZipFile;
        }
        if (type == eCompress) {
            processor = new CZipStreamCompressor(ICompression::eLevel_Default, flags);
        } else {
            processor = new CZipStreamDecompressor(flags);
        }
        break;
    }

    return processor;
}


END_NCBI_SCOPE
