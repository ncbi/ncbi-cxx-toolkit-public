/* $Id$
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
 * Author: Christiam Camacho
 *
 */

/** @file split_query_blk.hpp
 * Definition of C++ wrapper for SSplitQueryBlk
 */

#ifndef ALGO_BLAST_API__SPLIT_QUERY_BLK_HPP
#define ALGO_BLAST_API__SPLIT_QUERY_BLK_HPP

#include <corelib/ncbiobj.hpp>
#include <util/range.hpp>
#include <algo/blast/core/split_query.h>

/** @addtogroup AlgoBlast
 *
 * @{
 */

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(blast)

/// Range describing a query chunk
typedef COpenRange<TSeqPos> TChunkRange;

/// Wrapper class around SSplitQueryBlk structure
class NCBI_XBLAST_EXPORT CSplitQueryBlk : public CObject {
public:
    /** 
    * @brief Constructor for wrapper class for SSplitQueryBlk
    * 
    * @param num_chunks number of chunks
    */
    CSplitQueryBlk(Uint4 num_chunks, bool gapped_merge = true);

    /// Destructor
    ~CSplitQueryBlk();

    /// Retrieve the number of chunks
    size_t GetNumChunks() const;
    /// Get the number of queries in a given chunk
    /// @param chunk_num desired chunk [in]
    size_t GetNumQueriesForChunk(size_t chunk_num) const;
    /// Get the indices of the queries contained in a given chunk
    /// @param chunk_num desired chunk [in]
    vector<size_t> GetQueryIndices(size_t chunk_num) const;
    /// Get the contexts of the queries contained in a given chunk
    /// @param chunk_num desired chunk [in]
    vector<int> GetQueryContexts(size_t chunk_num) const;
    /// Get the context offsets (corrections) of the queries contained in a 
    /// given chunk
    /// @param chunk_num desired chunk [in]
    vector<size_t> GetContextOffsets(size_t chunk_num) const;

    /// Get the boundaries of a chunk in the concatenated query
    /// @param chunk_num desired chunk [in]
    TChunkRange GetChunkBounds(size_t chunk_num) const;
    /// Set the boundaries of a chunk in the concatenated query
    /// @param chunk_num desired chunk [in]
    /// @param chunk_range range in the concatenated query for this chunk [in]
    void SetChunkBounds(size_t chunk_num, const TChunkRange& chunk_range);
    /// Adds a query index to a given chunk
    /// @param chunk_num desired chunk [in]
    /// @param query_index index of query to be added [in]
    void AddQueryToChunk(size_t chunk_num, Int4 query_index);
    /// Adds a query context to a given chunk
    /// @param chunk_num desired chunk [in]
    /// @param context_index context of the concatenated query to be added [in]
    void AddContextToChunk(size_t chunk_num, Int4 context_index);
    /// Adds a context offset (correction) to a given chunk
    /// @param chunk_num desired chunk [in]
    /// @param context_offset query context offset (correction) to be added [in]
    void AddContextOffsetToChunk(size_t chunk_num, Int4 context_offset);

    /// Returns the C structure managed by objects of this class
    /// @note the caller of this method does NOT own the returned structure
    SSplitQueryBlk* GetCStruct() const;

    /// Sets the size (# of bases/residues) of overlap between query chunks
    /// @param size value to set [in]
    void SetChunkOverlapSize(size_t size);
    /// Gets the size (# of bases/residues) of overlap between query chunks
    size_t GetChunkOverlapSize() const;

    /// Print this object so that its contents can be directly used to update
    /// split_query.ini (for unit testing)
    /// @param out stream to print this object [in|out]
    /// @param rhs object to print [in]
    friend ostream& operator<<(ostream& out, const CSplitQueryBlk& rhs);

private:
    /// The structure this object manages
    SSplitQueryBlk* m_SplitQueryBlk;

    /// Do not allow copy-construction
    CSplitQueryBlk(const CSplitQueryBlk& rhs);
    /// Do not allow assignment operator
    CSplitQueryBlk& operator=(const CSplitQueryBlk& rhs);
};

END_SCOPE(BLAST)
END_NCBI_SCOPE

/* @} */

#endif /* ALGO_BLAST_API__SPLIT_QUERY_BLK__HPP */

