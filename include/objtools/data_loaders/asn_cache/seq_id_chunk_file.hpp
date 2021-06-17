#ifndef ASN_CACHE_SEQ_ID_CHUNK_FILE_HPP__
#define ASN_CACHE_SEQ_ID_CHUNK_FILE_HPP__
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
 * Authors:  Cheinan Marks
 *
 * File Description:
 * Represent a chunk file for the SeqId info -- a part of the ASN Cache ISAM storage model.
 * Unlike CChunkFile, the data here is small enough that the file should never get too
 * large, so there's no need to have more than one chunk file.
 */

#include <string>
#include <vector>
#include <fstream>

#include <corelib/ncbifile.hpp>

#include <util/simple_buffer.hpp>

#include <serial/objostrasnb.hpp>

#include <objtools/data_loaders/asn_cache/file_names.hpp>

#include <objects/seq/seq_id_handle.hpp>
#include <objects/seq/Bioseq.hpp>

BEGIN_NCBI_SCOPE

class CSeqIdChunkFile : public CFile
{
public:
    CSeqIdChunkFile(const std::string & root_path = "")
    : m_OpenFileRootPath(root_path)
    { x_ReserveBufferSpace(); }

    void    OpenForWrite( const std::string & root_path = "");
    void    OpenForRead( const std::string & root_path = "");
    void    Write( const objects::CBioseq::TId & seq_ids );
    void    Write( const vector<objects::CSeq_id_Handle> & seq_ids );
    void    RawWrite( const char * raw_seq_ids, size_t raw_seq_ids_size );
    void    Read( vector<objects::CSeq_id_Handle>& target, std::streampos offset, size_t seq_ids_size );
    void    RawRead( std::streampos offset, char * raw_seq_ids, size_t raw_seq_ids_size );
    bool    Append( const string & root_path,
                    const CFile & input_seq_id_chunk_file,
                    Uint8 input_offset = 0 );
    Int8    GetOffset() { return m_FileStream.tellp(); }

private:
    CNcbiFstream    m_FileStream;
    CSimpleBufferT<char>   m_Buffer;
    std::string     m_OpenFileRootPath;

    void    x_ReserveBufferSpace() { m_Buffer.reserve(10 * 1024); }
};

END_NCBI_SCOPE
#endif  // ASN_CACHE_SEQ_ID_CHUNK_FILE_HPP__

