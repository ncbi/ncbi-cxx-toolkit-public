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
 * Merge a set of CAsnIndex indices whose paths are passed in the constructor.
 *
 */

#include <ncbi_pch.hpp>

#include <string>
#include <functional>
#include <algorithm>

#include <db/bdb/bdb_cursor.hpp>

#include <objtools/data_loaders/asn_cache/file_names.hpp>
#include <objtools/data_loaders/asn_cache/asn_cache.hpp>
#include "merge_sub_indices.hpp"


USING_NCBI_SCOPE;

//  This is your garden variety merge of a list of sorted lists.
//  Set up a heap with the first entry of every list, pop off
//  the top entry from the heap and write it to the main index.
//  Replace it with an item from the list that was written to
//  the main index.
std::string CMergeSubIndices::operator() ()
{
    x_OpenSubIndices();
    x_LoadFirstEntries();
    x_Merge();
    LOG_POST( Info << m_BDBStopwatch.Elapsed() << " seconds for BDB." );
    LOG_POST( Info << m_QueueStopwatch << " seconds using priority queue." );
    
    return  m_MainIndex.GetFileName();
}

void CMergeSubIndices::WriteIndexEntry(const SIndexLocator &index_entry)
{
#ifdef _DEBUG
        LOG_POST(Info << "Creating index entry " << index_entry.AsString() << ": " << index_entry);
#endif
        m_MainIndex.SetSeqId( index_entry.m_SeqId );
        m_MainIndex.SetVersion( index_entry.m_Version );
        m_MainIndex.SetGi( index_entry.m_GI );
        m_MainIndex.SetTimestamp( index_entry.m_Timestamp );
        m_MainIndex.SetChunkId( index_entry.m_ChunkId );
        m_MainIndex.SetOffset( index_entry.m_Offset );
        m_MainIndex.SetSize( index_entry.m_ChunkSize );
        m_MainIndex.SetSeqLength( index_entry.m_SeqLength );
        m_MainIndex.SetTaxId( index_entry.m_TaxId );
        if ( eBDB_Ok != m_MainIndex.UpdateInsert() ) {
            LOG_POST( Error << "Main index failed to index SeqId "
                        << index_entry.m_SeqId );
        }

        if(!index_entry.HasSeqIdIndexData())
            return;

        /// This entry has SeqId index data, so write out an index entry for it
        m_SeqIdIndex.SetSeqId( index_entry.m_SeqId );
        m_SeqIdIndex.SetVersion( index_entry.m_Version );
        m_SeqIdIndex.SetGi( index_entry.m_GI );
        m_SeqIdIndex.SetTimestamp( index_entry.m_Timestamp );
        m_SeqIdIndex.SetOffset( index_entry.m_SeqIdOffset );
        m_SeqIdIndex.SetSize( index_entry.m_SeqIdChunkSize );
        if ( eBDB_Ok != m_SeqIdIndex.UpdateInsert() ) {
            LOG_POST( Error << "SeqId index failed to index SeqId "
                        << index_entry.m_SeqId );
        }
}


void    CMergeSubIndices::x_OpenSubIndices()
{
    ITERATE( std::vector<std::string>, subindex_itr, m_SubIndexList ) {
        m_SubIndices[subindex_itr - m_SubIndexList.begin()].reset(
              new CNcbiIfstream(subindex_itr->c_str()));
    }
    LOG_POST( Info << "Opened " << m_SubIndexList.size() << " sub indices." );
}



void    CMergeSubIndices::x_LoadFirstEntries()
{
    size_t  array_index = 0;
    for(size_t subindex = 0; subindex < m_SubIndexList.size(); ++subindex)
    {
            SQueueEntry   queue_entry;
            queue_entry.m_IndexEntry.m_SeqId = m_SeqIdBlob +
                                   array_index * SIndexLocator::k_MaxSeqIdLength;
            queue_entry.m_IndexHandle = array_index++;
            
            if(!queue_entry.m_IndexEntry.ReadFrom(*m_SubIndices[subindex]))
                continue;

            m_QueueStopwatch.Start();
            m_IndexEntryQueue.push( queue_entry );
            m_QueueStopwatch.Stop();
    }
}



void    CMergeSubIndices::x_Merge()
{
    unsigned int    entry_count = 0;
    while( ! m_IndexEntryQueue.empty() ) {
        entry_count++;
        SQueueEntry queue_entry = m_IndexEntryQueue.top();
        
        m_BDBStopwatch.Start();
        WriteIndexEntry(queue_entry.m_IndexEntry);
        m_BDBStopwatch.Stop();

        if ( queue_entry.m_IndexEntry.ReadFrom(*m_SubIndices[queue_entry.m_IndexHandle]) ) {
#ifdef _DEBUG
            LOG_POST(Info << "Read intermediate index entry " << queue_entry.m_IndexEntry.AsString() << ": " << queue_entry.m_IndexEntry << " intermediate file location " << m_SubIndices[queue_entry.m_IndexHandle]->tellg());
#endif
            m_QueueStopwatch.Start();
            m_IndexEntryQueue.pop();
            m_IndexEntryQueue.push( queue_entry );
            m_QueueStopwatch.Stop();
        } else {
            LOG_POST( Error << "Closing subindex " << queue_entry.m_IndexHandle
                        << ".  " << entry_count << " entries processed." );
            m_QueueStopwatch.Start();
            m_IndexEntryQueue.pop();
            m_QueueStopwatch.Stop();
        }
    }
}
