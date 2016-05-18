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
 *   Produce an ASN.1 cache index from CSeq_entry blobs passed from ID.
 *   This module is built into a library designed to be linked into an
 *   ID team machine with direct access to the databases.
 */

#include <ncbi_pch.hpp>
#include <ctime>
#include <sstream>
#include <algorithm>
#include <string>
#include <limits>

#include <corelib/ncbitime.hpp>
#include <corelib/rwstream.hpp>
#include <corelib/ncbifile.hpp>

#include <util/compress/stream.hpp>
#include <util/compress/zlib.hpp>

#include <objects/seqset/Seq_entry.hpp>
#include <objects/seq/Bioseq.hpp>
#include <objects/seqset/Bioseq_set.hpp>
#include <objects/seqloc/Seq_id.hpp>
#include <objects/seq/Seq_descr.hpp>
#include <objects/seqfeat/BioSource.hpp>
#include <objects/seqfeat/Org_ref.hpp>
#include <objects/general/Dbtag.hpp>
#include <objects/general/Object_id.hpp>
#include <objects/seq/Seq_inst.hpp>

#include <serial/serial.hpp>
#include <serial/objostrasnb.hpp>

#include "md5_writer.hpp"

#include <objtools/data_loaders/asn_cache/chunk_file.hpp>
#include <objtools/data_loaders/asn_cache/seq_id_chunk_file.hpp>
#include <objtools/data_loaders/asn_cache/file_names.hpp>
#include <objtools/data_loaders/asn_cache/asn_cache_exception.hpp>
#include <objtools/data_loaders/asn_cache/asn_cache_util.hpp>
#include <objtools/data_loaders/asn_cache/dump_asn_index.hpp>

BEGIN_NCBI_SCOPE
USING_SCOPE( objects );

void    CDumpASNIndex::DumpBlob ( const CRef<CSeq_entry> entry,
                                    const CTime & timestamp,
                                    SSatSatKey /*blob_id*/ )
{
    m_DumpSW.Start();

    CRef<CSeq_entry> parentized_entry = entry;
    parentized_entry->Parentize();
    m_CacheBlob.Reset();
    x_CreateBlob( *parentized_entry, timestamp);
    m_ChunkFile.OpenForWrite( m_RootDirPath );
    x_WriteBlob();
    m_SeqIdChunkFile.OpenForWrite( m_RootDirPath );
    x_BuildIndexAndSeqIdInfo( *parentized_entry );

    m_DumpSW.Stop();
}


void    CDumpASNIndex::Done()
{
}


void    CDumpASNIndex::x_CreateBlob ( const CSeq_entry & entry, const CTime & timestamp )
{
    m_CreateBlobSW.Start();
    
    // CAsnCache can't store dates after Jan 18 2038. There is compiler time check
    // that should stop compilation in 2035. But I was asked to avoid it,
    // since we still support pre-C++11 compilers
    //static_assert ( !(__DATE__[9] >= '3' && __DATE__ [10] >= '5'), "AsnCache doesn't support dates after Jan 18 2038. Today is " __DATE__ );
        
    m_CacheBlob.SetTimestamp( timestamp.GetTimeT() );
    m_CacheBlob.Pack(entry);

    m_CreateBlobSW.Stop();
}


void    CDumpASNIndex::x_CreateRootDir()
{
    m_RootDirPath = CDirEntry::ConcatPath( m_RootDirPath, m_SatId.AsString() );
    CDir    root_dir( m_RootDirPath );
    if ( root_dir.Exists() ) {
        //  Do nothing.
    } else {
        if ( ! root_dir.CreatePath() ) {
            int saved_errno = NCBI_ERRNO_CODE_WRAPPER();
            std::string error_string = "Attempted path was \"" + m_RootDirPath;
            error_string += "\". errno was " + NStr::IntToString( saved_errno );
            error_string += ": " + std::string( NCBI_ERRNO_STR_WRAPPER( saved_errno ) );

            NCBI_THROW( CASNCacheException, eRootDirectoryCreationFailed, error_string );
        }
    }
}


void    CDumpASNIndex::x_WriteBlob()
{
    m_WriteBlobSW.Start();

    m_Offset = m_ChunkFile.GetOffset();
    m_ChunkFile.Write( m_CacheBlob );
    m_Size = m_ChunkFile.GetOffset() - m_Offset;

    LOG_POST(Info << "Blob written @ chunk " << m_ChunkFile.GetChunkSerialNum()
        << ", offset " << m_Offset << ", size " << m_Size );

    m_WriteBlobSW.Stop();
}


void    CDumpASNIndex::x_BuildIndexAndSeqIdInfo( const objects::CSeq_entry & entry )
{
    m_BuildIndexEntrySW.Start();

    if ( entry.IsSet() ) {
        ITERATE (CSeq_entry::TSet::TSeq_set, iter, entry.GetSet().GetSeq_set()) {
            x_BuildIndexAndSeqIdInfo( **iter );
        }
    } else if ( entry.IsSeq() ) {
        CAsnIndex::TTimestamp   timestamp = m_CacheBlob.GetTimestamp();
        CAsnIndex::TChunkId     chunk_id = m_ChunkFile.GetChunkSerialNum();
        const CBioseq & bio_seq = entry.GetSeq();
        if (m_Size > numeric_limits<CAsnIndex::TSize>::max()) {
            NCBI_THROW( CASNCacheException, eRootDirectoryCreationFailed,
                        "Sequence is to big (AsnCache supports only objects <= 4Gb)" );
        }
        IndexABioseq( bio_seq, m_MainIndex, timestamp, chunk_id, m_Offset, CAsnIndex::TSize(m_Size) );
        Int8 seq_id_offset = m_SeqIdChunkFile.GetOffset();
        m_SeqIdChunkFile.Write( bio_seq.GetId() );
        size_t size = m_SeqIdChunkFile.GetOffset() - seq_id_offset;
        if  (size > numeric_limits<CAsnIndex::TSize>::max()) {
            NCBI_THROW( CASNCacheException, eRootDirectoryCreationFailed,
                        "Sequence is to big (AsnCache supports only objects <= 4Gb)" );
        }
        IndexABioseq( bio_seq, m_SeqIdIndex, timestamp, 0,
                      seq_id_offset, CAsnIndex::TSize(size) );
    } else {
        //  This should never happen.
        _ASSERT( false );
    }

    m_BuildIndexEntrySW.Stop();
}


Uint4    CDumpASNIndex::x_GetTaxId( const CBioseq & bio_seq )
{
    Uint4 taxid = 0;
    if (bio_seq.IsSetDescr()  &&  bio_seq.GetDescr().IsSet()) {
        ITERATE (CBioseq::TDescr::Tdata, it, bio_seq.GetDescr().Get()) {
            const CSeqdesc& desc = **it; 
            const COrg_ref* org = NULL; 
            switch (desc.Which()) {
            case CSeqdesc::e_Org:
                org = &desc.GetOrg();
                break;  

            case CSeqdesc::e_Source:
                org = &desc.GetSource().GetOrg();
                break;  

            default:
                break;  
            }       
            if (org  &&  org->IsSetDb()) {
                ITERATE (COrg_ref::TDb, dbiter, org->GetDb()) {
                    if ((*dbiter)->GetDb() == "taxon") {
                        taxid = (*dbiter)->GetTag().GetId();
                        break;  
                    }       
                }       
            }       

            if (taxid) {
                break;  
            }       
        }       
    }       

    return  taxid;
}


void    CDumpASNIndex::x_WriteHeader()
{
    CNcbiOfstream   header_stream( CDirEntry::ConcatPath( m_RootDirPath,
        NASNCacheFileName::GetHeader() ).c_str(), std::ios::binary );
    header_stream.write( reinterpret_cast<const char *>( &kMajorVersion ), sizeof( kMajorVersion ) );
    header_stream.write( reinterpret_cast<const char *>( &kMinorVersion ), sizeof( kMinorVersion ) );
    std::string build_id = "$Id$";
    header_stream << build_id;
}


void    CDumpASNIndex::DumpTiming()
{
    if ( m_CreateBlobSW.EIsInstantiated ) {
        LOG_POST( Info << "CreateBlob stopwatch measured " << m_CreateBlobSW.Elapsed()
                    << " seconds." );
    }
    if ( m_WriteBlobSW.EIsInstantiated ) {
        LOG_POST( Info << "WriteBlob stopwatch measured " << m_WriteBlobSW.Elapsed() << " seconds." );
    }
    if ( m_BuildIndexEntrySW.EIsInstantiated ) {
        LOG_POST( Info << "BuildIndexEntry stopwatch measured " << m_BuildIndexEntrySW.Elapsed()
                    << " seconds." );
    }
    if ( m_DumpSW.EIsInstantiated ) {
        LOG_POST( Info << "Dump stopwatch measured " << m_DumpSW.Elapsed() << " seconds." );
    }
}

END_NCBI_SCOPE

