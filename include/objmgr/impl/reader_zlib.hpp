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


BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

class NCBI_XOBJMGR_EXPORT CResultZBtSrc : public CByteSource
{
public:
    CResultZBtSrc(CByteSourceReader* src);
    ~CResultZBtSrc();

    virtual CRef<CByteSourceReader> Open(void);

private:
    CResultZBtSrc(const CResultZBtSrc&);
    const CResultZBtSrc& operator=(const CResultZBtSrc&);

    CRef<CByteSourceReader> m_Src;
};


class CResultZBtSrcX;
class CResultZBtSrcRdr;


class NCBI_XOBJMGR_EXPORT CResultZBtSrcRdr : public CByteSourceReader
{
public:
    CResultZBtSrcRdr(CByteSourceReader* src);
    ~CResultZBtSrcRdr();

    enum EType {
        eType_unknown,
        eType_plain,
        eType_zlib
    };

    virtual size_t Read(char* buffer, size_t bufferLength);
    size_t GetCompressedSize(void) const;

private:
    CResultZBtSrcRdr(const CResultZBtSrcRdr&);
    const CResultZBtSrcRdr& operator=(const CResultZBtSrcRdr&);

    CRef<CByteSourceReader> m_Src;
    EType       m_Type;
    auto_ptr<CResultZBtSrcX> m_Decompressor;
};


END_SCOPE(objects)
END_NCBI_SCOPE


/*
* $Log$
* Revision 1.3  2003/10/14 22:35:09  ucko
* Add missing declaration of CResultZBtSrcRdr::GetCompressedSize.
*
* Revision 1.2  2003/07/24 20:41:43  vasilche
* Added private constructor to make MSVC-DLL happy.
*
* Revision 1.1  2003/07/24 19:28:08  vasilche
* Implemented SNP split for ID1 loader.
*
*
*/

#endif // READER_ZLIB__HPP_INCLUDED
