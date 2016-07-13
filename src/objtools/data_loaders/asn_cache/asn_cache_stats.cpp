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

#include <objtools/data_loaders/asn_cache/asn_cache.hpp>
#include <objtools/data_loaders/asn_cache/asn_index.hpp>
#include <objtools/data_loaders/asn_cache/asn_cache_stats.hpp>


USING_NCBI_SCOPE;

size_t    CAsnCacheStats::GetGICount( CNcbiOstream * stream ) const
{
    std::set<long>    gi_set;
    CBDB_FileCursor cursor( m_AsnIndexRef );
    cursor.SetCondition(CBDB_FileCursor::eFirst, CBDB_FileCursor::eLast);
    while (cursor.Fetch() == eBDB_Ok) {
        long    gi = m_AsnIndexRef.GetGi();
        gi_set.insert( gi );
        if ( stream ) {
            *stream << gi << '\n';
        }
    }

    return gi_set.size();
}


void    CAsnCacheStats::DumpSeqIds( CNcbiOstream & stream ) const
{
    CBDB_FileCursor cursor( m_AsnIndexRef );
    cursor.SetCondition(CBDB_FileCursor::eFirst, CBDB_FileCursor::eLast);
    unsigned long   seqid_count = 0;
    while (cursor.Fetch() == eBDB_Ok) {
        stream << m_AsnIndexRef.GetSeqId() << " | "
            << m_AsnIndexRef.GetVersion() << " | "
            << m_AsnIndexRef.GetGi();
        if (m_IncludeFlags & eIncludeTimestamp) {
            stream << " | " << m_AsnIndexRef.GetTimestamp();
        }
        stream << '\n';
        seqid_count++;
    }
    
    stream << "Total " << seqid_count << " seq_ids found\n.";
}



void    CAsnCacheStats::DumpIndex( CNcbiOstream & stream ) const
{
    CBDB_FileCursor cursor( m_AsnIndexRef );
    cursor.SetCondition(CBDB_FileCursor::eFirst, CBDB_FileCursor::eLast);
    unsigned long   seqid_count = 0;
    while (cursor.Fetch() == eBDB_Ok) {
        stream << m_AsnIndexRef.GetSeqId() << " | "
            << m_AsnIndexRef.GetVersion() << " | "
            << m_AsnIndexRef.GetGi();
        if (m_IncludeFlags & eIncludeTimestamp) {
            stream << " | " << m_AsnIndexRef.GetTimestamp();
        }
        if (m_IncludeFlags & eIncludeLocation) {
            stream << " | " << m_AsnIndexRef.GetChunkId() << " | "
                << m_AsnIndexRef.GetOffset() << " | "
                << m_AsnIndexRef.GetSize();
        }
        stream << " | "
            << m_AsnIndexRef.GetSeqLength() << " | "
            << m_AsnIndexRef.GetTaxId()
            << '\n';
        seqid_count++;
    }
    
    stream << "Total " << seqid_count << " seq_ids found\n.";
}



