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
 *  Author:  Anton Butanaev, Eugene Vasilchenko
 *
 *  File Description: Base data reader interface
 *
 */

#include <ncbi_pch.hpp>
#include <objtools/data_loaders/genbank/writer.hpp>
#include <objtools/data_loaders/genbank/processor.hpp>

#include <objmgr/objmgr_exception.hpp>
#include <util/bytesrc.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

CWriter::~CWriter(void)
{
}


void CWriter::WriteInt(CNcbiOstream& stream, int value)
{
    stream.write(reinterpret_cast<const char*>(&value), sizeof(value));
}


void CWriter::WriteBytes(CNcbiOstream& stream,
                         CRef<CByteSource> byte_source)
{
    WriteBytes(stream, byte_source->Open());
}


void CWriter::WriteBytes(CNcbiOstream& stream,
                         CRef<CByteSourceReader> reader)
{
    const size_t BUFFER_SIZE = 8*1024;
    char buffer[BUFFER_SIZE];
    for ( ;; ) {
        size_t cnt = reader->Read(buffer, BUFFER_SIZE);
        if ( cnt == 0 ) {
            if ( reader->EndOfData() ) {
                break;
            }
            else {
                NCBI_THROW(CLoaderException, eLoaderFailed,
                           "Cannot store loaded blob in cache");
            }
        }
        stream.write(buffer, cnt);
    }
}


void CWriter::WriteBytes(CNcbiOstream& stream,
                         const TOctetStringSequence& data)
{
    ITERATE ( TOctetStringSequence, it, data ) {
        WriteBytes(stream, **it);
    }
}


void CWriter::WriteBytes(CNcbiOstream& stream,
                         const TOctetString& data)
{
    if ( !data.empty() ) {
        stream.write(&data[0], data.size());
    }
}


void CWriter::WriteProcessorTag(CNcbiOstream& stream,
                                const CProcessor& processor)
{
    WriteInt(stream, processor.GetType());
    WriteInt(stream, processor.GetMagic());
}


CWriter::CBlobStream::~CBlobStream(void)
{
}


CRef<CWriter::CBlobStream>
CWriter::OpenBlobStream(CReaderRequestResult& /*result*/,
                        const TBlobId& /*blob_id*/,
                        const CProcessor& /*processor*/)
{
    return null;
}


CRef<CWriter::CBlobStream>
CWriter::OpenChunkStream(CReaderRequestResult& /*result*/,
                         const TBlobId& /*blob_id*/,
                         TChunkId /*chunk_id*/,
                         const CProcessor& /*processor*/)
{
    return null;
}


END_SCOPE(objects)
END_NCBI_SCOPE
