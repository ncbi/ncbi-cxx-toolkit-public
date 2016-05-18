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
 * Authors:  Mike DiCuccio Cheinan Marks
 *
 * File Description:
 *
 */

#include <ncbi_pch.hpp>

#include <string>
#include <fstream>

#include <objtools/data_loaders/asn_cache/asn_cache_util.hpp>
#include <objects/general/Dbtag.hpp>

#include <objects/seqset/Seq_entry.hpp>
#include <objects/seqset/Bioseq_set.hpp>

BEGIN_NCBI_SCOPE
USING_SCOPE(objects);


void GetNormalizedSeqId(const CSeq_id& id,
                        string& id_str, Uint4& version)
{
    id.GetLabel(&id_str, CSeq_id::eType);
    id_str += '|';
    if (id.IsGeneral()) {
        id_str += id.GetGeneral().GetDb();
        id_str += "|";
    }
    id.GetLabel(&id_str,
                CSeq_id::eContent,
                CSeq_id::fLabel_GeneralDbIsContent);

    NStr::ToLower(id_str);

    version = 0;
    const CTextseq_id* text_id = id.GetTextseq_Id();
    if (text_id  &&  text_id->IsSetVersion()) {
        version = text_id->GetVersion();
    }
}



void GetNormalizedSeqId(const objects::CSeq_id_Handle& id,
                        string& id_str, Uint4& version)
{
    GetNormalizedSeqId(*id.GetSeqId(), id_str, version);
}



void    ReadThroughFile( const std::string & file_path )
{
    /// Read through a file and throw away the data.  This optimizes
    /// random access performance when using panfs.
    CNcbiIfstream   dump_index_stream( file_path.c_str(), std::ios::binary );
    const size_t    kBufferSize = 64 * 1024 * 1024;
    std::vector<char> buffer( kBufferSize );
    while ( dump_index_stream ) {
        dump_index_stream.read( &buffer[0], kBufferSize );
    }
}

///
/// Extract bioseq with given id from entry
///
CConstRef<CBioseq> ExtractBioseq(CConstRef<CSeq_entry> entry,
                                 const CSeq_id_Handle & id)
{
    CConstRef<CBioseq> bioseq;
    if (entry) {
        if(entry->IsSet()){
            ITERATE(CBioseq_set::TSeq_set, it, entry->GetSet().GetSeq_set())
                if((bioseq = ExtractBioseq(*it, id)).NotNull())
                    break;
        } else {
            ITERATE(CBioseq::TId, it, entry->GetSeq().GetId())
                if(id.GetSeqId()->Match(**it)){
                    bioseq = &entry->GetSeq();
                    break;
                }
        }
    }
    return bioseq;
}





END_NCBI_SCOPE

