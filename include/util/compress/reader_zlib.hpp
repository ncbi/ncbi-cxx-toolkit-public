#ifndef READER_ZLIB__HPP_INCLUDED
#define READER_ZLIB__HPP_INCLUDED

/*  $Id$
 * ===========================================================================
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
 * ===========================================================================
 *
 *  Author:  Eugene Vasilchenko
 *
 *  File Description: byte reader with gzip decompression
 *
 */

#include <corelib/ncbiobj.hpp>
#include <util/bytesrc.hpp>
#include <memory>


/** @addtogroup Compression
 *
 * @{
 */


BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)


class CResultZBtSrcX;
class CResultZBtSrcRdr;


class NCBI_XUTIL_EXPORT CNlmZipBtRdr : public CByteSourceReader
{
public:
    CNlmZipBtRdr(CByteSourceReader* src);
    ~CNlmZipBtRdr();

    enum EType {
        eType_unknown,
        eType_plain,
        eType_zlib
    };

    virtual size_t Read(char* buffer, size_t bufferLength);
    virtual bool Pushback(const char* data, size_t size);

    size_t GetCompressedSize(void) const;
    double GetDecompressionTime(void) const;
    double GetTotalReadTime(void) const
        {
            return m_TotalReadTime;
        }

private:
    CNlmZipBtRdr(const CNlmZipBtRdr&);
    const CNlmZipBtRdr& operator=(const CNlmZipBtRdr&);

    CRef<CByteSourceReader>  m_Src;
    EType                    m_Type;
    auto_ptr<CResultZBtSrcX> m_Decompressor;
    double                   m_TotalReadTime;
};


END_SCOPE(objects)
END_NCBI_SCOPE


/* @} */


/*
 * ===========================================================================
 * $Log$
 * Revision 1.3  2004/08/09 15:59:07  vasilche
 * Implemented CNlmZipBtRdr::Pushback() for uncompressed data.
 *
 * Revision 1.2  2004/04/07 16:12:00  ivanov
 * Cosmeic changes. Fixed header and footer.
 *
 * Revision 1.1  2003/12/23 17:01:32  kuznets
 *  Moved from objmgr to util/compress
 *
 * Revision 1.5  2003/10/21 14:27:35  vasilche
 * Added caching of gi -> sat,satkey,version resolution.
 * SNP blobs are stored in cache in preprocessed format (platform dependent).
 * Limit number of connections to GenBank servers.
 * Added collection of ID1 loader statistics.
 *
 * Revision 1.4  2003/10/15 13:43:57  vasilche
 * Removed obsolete class CResultZBtSrc.
 * Some code formatting.
 *
 * Revision 1.3  2003/10/14 22:35:09  ucko
 * Add missing declaration of CResultZBtSrcRdr::GetCompressedSize.
 *
 * Revision 1.2  2003/07/24 20:41:43  vasilche
 * Added private constructor to make MSVC-DLL happy.
 *
 * Revision 1.1  2003/07/24 19:28:08  vasilche
 * Implemented SNP split for ID1 loader.
 *
 * ===========================================================================
 */ 

#endif // READER_ZLIB__HPP_INCLUDED
