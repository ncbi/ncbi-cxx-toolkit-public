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
#include <objmgr/split/asn_sizer.hpp>

#include <serial/objostr.hpp>

#include <objmgr/split/id2_compress.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)


CAsnSizer::CAsnSizer(void)
{
}


CAsnSizer::~CAsnSizer(void)
{
}


CObjectOStream& CAsnSizer::OpenDataStream(void)
{
    m_AsnData.clear();
    m_CompressedData.clear();
    m_OStream.reset();
    m_MStream.reset(new CNcbiOstrstream);
    m_OStream.reset(CObjectOStream::Open(eSerial_AsnBinary, *m_MStream));
    return *m_OStream;
}


void CAsnSizer::CloseDataStream(void)
{
    m_OStream.reset();
    size_t size = m_MStream->pcount();
    const char* data = m_MStream->str();
    m_MStream->freeze(false);
    m_AsnData.assign(data, data+size);
    m_MStream.reset();
}


size_t CAsnSizer::GetCompressedSize(const SSplitterParams& params)
{
    CId2Compressor::Compress(params, m_CompressedData,
                             GetAsnData(), GetAsnSize());
    return GetCompressedSize();
}


END_SCOPE(objects)
END_NCBI_SCOPE

/*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.5  2004/05/21 21:42:13  gorelenk
* Added PCH ncbi_pch.hpp
*
* Revision 1.4  2004/01/07 17:36:21  vasilche
* Moved id2_split headers to include/objmgr/split.
* Fixed include path to genbank.
*
* Revision 1.3  2003/12/30 16:06:13  vasilche
* Compression methods moved to separate header: id2_compress.hpp.
*
* Revision 1.2  2003/11/26 23:04:56  vasilche
* Removed extra semicolons after BEGIN_SCOPE and END_SCOPE.
*
* Revision 1.1  2003/11/12 16:18:24  vasilche
* First implementation of ID2 blob splitter withing cache.
*
* ===========================================================================
*/
