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


// Algorithm-specific default flags

#if defined(HAVE_LIBBZ2)
    const ICompression::TFlags kDefault_BZip2               = 0;
#endif 
#if defined(HAVE_LIBLZO)
    const ICompression::TFlags kDefault_LZO                 = 0;
#endif 
#if defined(HAVE_LIBZ)
    const ICompression::TFlags kDefault_Zip                 = 0;
    const ICompression::TFlags kDefault_GZipFile            = CZipCompression::fGZip;
#endif 
#if defined(HAVE_LIBZCF)
    const ICompression::TFlags kDefault_ZipCloudflare       = 0;
    const ICompression::TFlags kDefault_GZipCloudflareFile  = CZipCloudflareCompression::fGZip;
#endif 
#if defined(HAVE_LIBZSTD)
    const ICompression::TFlags kDefault_Zstd                = 0;
#endif 


bool CCompressStream::HaveSupport(EMethod method)
{
    switch(method) {
    case CCompressStream::eNone:
        return true;
    case CCompressStream::eBZip2:
        #if defined(HAVE_LIBBZ2)
            return true;
        #endif 
        break;
    case CCompressStream::eLZO:
        #if defined(HAVE_LIBLZO)
            return true;
        #endif 
        break;
    case CCompressStream::eZip:
    case CCompressStream::eGZipFile:
    case CCompressStream::eConcatenatedGZipFile:
        #if defined(HAVE_LIBZ)
            return true;
        #endif 
        break;
    case CCompressStream::eZipCloudflare:
    case CCompressStream::eGZipCloudflareFile:
        #if defined(HAVE_LIBZCF)
            return true;
        #endif 
        break;
    case CCompressStream::eZstd:
        #if defined(HAVE_LIBZSTD)
            return true;
        #endif
        break;
    }
    return false;
}


// Type of initialization
enum EInitType { 
    eCompress,
    eDecompress
};


CCompressionStreamProcessor* s_Init(EInitType                type,
                                    CCompressStream::EMethod method, 
                                    ICompression::TFlags     flags,
                                    ICompression::ELevel     level)
{
    CCompressionStreamProcessor* processor = 0;

    switch(method) {
    case CCompressStream::eNone:
        processor = new CTransparentStreamProcessor();
        break;

    case CCompressStream::eBZip2:
#if defined(HAVE_LIBBZ2)
        if (flags == CCompressStream::fDefault) {
            flags = kDefault_BZip2;
        } else {
            flags |= kDefault_BZip2;
        }
        if (type == eCompress) {
            processor = new CBZip2StreamCompressor(level, flags);
        } else {
            processor = new CBZip2StreamDecompressor(flags);
        }
#else
         NCBI_THROW(CCompressionException, eCompression, "BZIP2 compression is not available");
#endif 
        break;

    case CCompressStream::eLZO:
#if defined(HAVE_LIBLZO)
        if (flags == CCompressStream::fDefault) {
            flags = kDefault_LZO;
        } else {
            flags |= kDefault_LZO;
        }
        if (type == eCompress) {
            processor = new CLZOStreamCompressor(level, flags);
        } else {
            processor = new CLZOStreamDecompressor(flags);
        }
#else
         NCBI_THROW(CCompressionException, eCompression, "LZO compression is not available");
#endif 
        break;

    case CCompressStream::eZip:
#if defined(HAVE_LIBZ)
        if (flags == CCompressStream::fDefault) {
            flags = kDefault_Zip;
        } else {
            flags |= kDefault_Zip;
        }
        if (type == eCompress) {
            processor = new CZipStreamCompressor(level, flags);
        } else {
            processor = new CZipStreamDecompressor(flags);
        }
#else
         NCBI_THROW(CCompressionException, eCompression, "ZLIB compression is not available");
#endif 
        break;

    case CCompressStream::eGZipFile:
    case CCompressStream::eConcatenatedGZipFile:
#if defined(HAVE_LIBZ)
        if (flags == CCompressStream::fDefault) {
            flags = kDefault_GZipFile;
        } else {
            flags |= kDefault_GZipFile;
        }
        if (type == eCompress) {
            processor = new CZipStreamCompressor(level, flags);
        } else {
            processor = new CZipStreamDecompressor(flags);
        }
#else
         NCBI_THROW(CCompressionException, eCompression, "ZLIB compression is not available");
#endif 
        break;

    case CCompressStream::eZipCloudflare:
#if defined(HAVE_LIBZCF)
        if (flags == CCompressStream::fDefault) {
            flags = kDefault_ZipCloudflare;
        } else {
            flags |= kDefault_ZipCloudflare;
        }
        if (type == eCompress) {
            processor = new CZipCloudflareStreamCompressor(level, flags);
        } else {
            processor = new CZipCloudflareStreamDecompressor(flags);
        }
#else
         NCBI_THROW(CCompressionException, eCompression, "Cloudflare ZLIB compression is not available");
#endif
        break;

    case CCompressStream::eGZipCloudflareFile:
#if defined(HAVE_LIBZCF)
        if (flags == CCompressStream::fDefault) {
            flags = kDefault_GZipCloudflareFile;
        } else {
            flags |= kDefault_GZipCloudflareFile;
        }
        if (type == eCompress) {
            processor = new CZipCloudflareStreamCompressor(level, flags);
        } else {
            processor = new CZipCloudflareStreamDecompressor(flags);
        }
#else
         NCBI_THROW(CCompressionException, eCompression, "Cloudflare ZLIB compression is not available");
#endif
        break;

    case CCompressStream::eZstd:
#if defined(HAVE_LIBZSTD)
        if (flags == CCompressStream::fDefault) {
            flags = kDefault_Zstd;
        } else {
            flags |= kDefault_Zstd;
        }
        if (type == eCompress) {
            processor = new CZstdStreamCompressor(level, flags);
        } else {
            processor = new CZstdStreamDecompressor(flags);
        }
#else
         NCBI_THROW(CCompressionException, eCompression, "ZSTD compression is not available");
#endif
        break;

    default:
        NCBI_THROW(CCompressionException, eCompression, "Unknown compression method");
    }

    return processor;
}


CCompressIStream::CCompressIStream(CNcbiIstream& stream, EMethod method, 
                                   ICompression::TFlags stm_flags,
                                   ICompression::ELevel level,
                                   ENcbiOwnership own_istream)
{
    CCompressionStreamProcessor* processor = s_Init(eCompress, method, stm_flags, level);
    if (processor) {
        Create(stream, processor,
               own_istream == eTakeOwnership ? CCompressionStream::fOwnAll : 
                                               CCompressionStream::fOwnProcessor);
    }
}


CCompressOStream::CCompressOStream(CNcbiOstream& stream, EMethod method, 
                                   ICompression::TFlags stm_flags, 
                                   ICompression::ELevel level,
                                   ENcbiOwnership own_ostream)
{
    CCompressionStreamProcessor* processor = s_Init(eCompress, method, stm_flags, level);
    if (processor) {
        Create(stream, processor, 
               own_ostream == eTakeOwnership ? CCompressionStream::fOwnAll : 
                                               CCompressionStream::fOwnProcessor);
    }
}


CDecompressIStream::CDecompressIStream(CNcbiIstream& stream, EMethod method, 
                                       ICompression::TFlags stm_flags,
                                       ENcbiOwnership own_instream)
{
    CCompressionStreamProcessor* processor = 
        s_Init(eDecompress, method, stm_flags, ICompression::eLevel_Default);
    if (processor) {
        Create(stream, processor, 
               own_instream == eTakeOwnership ? CCompressionStream::fOwnAll : 
                                                CCompressionStream::fOwnProcessor);
    }
}


CDecompressOStream::CDecompressOStream(CNcbiOstream& stream, EMethod method, 
                                       ICompression::TFlags stm_flags,
                                       ENcbiOwnership own_ostream)
{
    CCompressionStreamProcessor* processor = 
        s_Init(eDecompress, method, stm_flags, ICompression::eLevel_Default);
    if (processor) {
        Create(stream, processor,
               own_ostream == eTakeOwnership ? CCompressionStream::fOwnAll : 
                                               CCompressionStream::fOwnProcessor);
    }
}


END_NCBI_SCOPE
