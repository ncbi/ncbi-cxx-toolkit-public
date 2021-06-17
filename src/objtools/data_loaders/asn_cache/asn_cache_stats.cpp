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
 *
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbistre.hpp>

#include <db/bdb/bdb_cursor.hpp>

#include <objtools/data_loaders/asn_cache/asn_cache_stats.hpp>
#include <objtools/data_loaders/asn_cache/asn_cache.hpp>

USING_NCBI_SCOPE;

size_t    CAsnCacheStats::GetGICount( CNcbiOstream * stream ) const
{
    if ( !stream ) {
        return m_AsnCacheRef.GetGiCount();
    }
    else {
        std::set<uint64_t> gi_coll;
        m_AsnCacheRef.EnumSeqIds([stream, &gi_coll](string /*seq_id*/, 
                                                        uint32_t /*version*/, 
                                                        uint64_t gi, 
                                                        uint32_t /*timestamp*/) {
            gi_coll.insert( gi );
            *stream << gi << '\n';
     
        });
        return gi_coll.size();
    }
}

void    CAsnCacheStats::DumpSeqIds( CNcbiOstream & stream ) const
{
    size_t seqid_count = 0;
 
    m_AsnCacheRef.EnumSeqIds([this, &stream, &seqid_count](string seq_id, 
                                                           uint32_t version, 
                                                           uint64_t gi, 
                                                           uint32_t timestamp) {
        stream << seq_id  << " | "
               << version << " | "
               << gi;

        if (m_IncludeFlags & eIncludeTimestamp) {
            stream << " | " << timestamp;
        }
        stream << '\n';

        seqid_count++;
    });
 
    stream << "Total " << seqid_count << " seq_ids found\n.";
}

void    CAsnCacheStats::DumpIndex( CNcbiOstream & stream ) const
{
    size_t seqid_count = 0;
 
    m_AsnCacheRef.EnumIndex([this, &stream, &seqid_count](string seq_id, 
                                                          uint32_t version, 
                                                          uint64_t gi, 
                                                          uint32_t timestamp, 
                                                          uint32_t chunk, 
                                                          uint32_t offset, 
                                                          uint32_t size, 
                                                          uint32_t seq_len, 
                                                          uint32_t taxid) {
                stream << seq_id  << " | "
               << version << " | "
               << gi;

        if (m_IncludeFlags & eIncludeTimestamp) {
            stream << " | " << timestamp;
        }

        if (m_IncludeFlags & eIncludeLocation) {
            stream << " | " 
                   << chunk << " | "
                   << offset << " | "
                   << size;
        }

        stream << " | "
               << seq_len << " | "
               << taxid
               << '\n';

        seqid_count++;
    });
 
    stream << "Total " << seqid_count << " seq_ids found\n.";
}



