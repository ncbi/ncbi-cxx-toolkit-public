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
    string s = CNcbiOstrstreamToString(*m_MStream);
    m_AsnData.assign(s.data(), s.data() + s.size());
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
