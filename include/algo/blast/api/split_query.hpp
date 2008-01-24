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
 * Author:  Christiam Camacho
 *
 */

/** @file split_query.hpp
 * Declares CQuerySplitter, a class to split the query sequence(s)
 */

#ifndef ALGO_BLAST_API__SPLIT_QUERY__HPP
#define ALGO_BLAST_API__SPLIT_QUERY__HPP

#include <algo/blast/api/query_data.hpp>

// Forward declarations
class CSplitQueriesTest;      // unit test class

/** @addtogroup AlgoBlast
 *
 * @{
 */

BEGIN_NCBI_SCOPE

// More forward declarations..
BEGIN_SCOPE(objects)
    class CScope;
END_SCOPE(objects)

BEGIN_SCOPE(blast)

// More forward declarations..
class CBlastQueryVector;

/// Class responsible for splitting query sequences and providing data to the
/// BLAST search class to search a split query chunk
class CQuerySplitter : public CObject
{
public:
    /// Defines a vector of CScope objects.
    typedef vector< CRef<objects::CScope> > TScopeVector;

    /// Parametrized constructor
    /// @param query_factory Object containing query sequence(s) [in]
    /// @param options BLAST options for the search [in]
    CQuerySplitter(CRef<IQueryFactory> query_factory, 
                   const CBlastOptions* options);

    /// Returns the number of bases/residues that make up a query chunk
    size_t GetChunkSize() const { return m_ChunkSize; }

    /// Returns the number of chunks the query/queries will be split into
    Uint4 GetNumberOfChunks() const { return m_NumChunks; }

    /// Determines whether the query sequence(s) are split or not
    bool IsQuerySplit() const { return GetNumberOfChunks() > 1; }

    /// Split the query sequence(s)
    CRef<class CSplitQueryBlk> Split();

    /// Returns a IQueryFactory suitable to be executed by a BLAST search class
    /// @param chunk_num chunk number to retrieve (< GetNumberOfChunks())
    CRef<IQueryFactory> GetQueryFactoryForChunk(Uint4 chunk_num);

    /// Print this object so that its contents can be directly used to update
    /// split_query.ini (for unit testing)
    /// @param out stream to print this object [in|out]
    /// @param rhs object to print [in]
    friend ostream& operator<<(ostream& out, const CQuerySplitter& rhs);

private:
    /// Definition of a vector of CBlastQueryVectors, each element corresponds
    /// to a query chunk
    typedef vector< CRef<CBlastQueryVector> > TSplitQueryVector;

    /// The original, unsplit query factory
    CRef<IQueryFactory> m_QueryFactory;
    /// BLAST options
    const CBlastOptions* m_Options;
    /// Number of chunks, if this is 1, no splitting occurs
    Uint4 m_NumChunks;
    /// Split query block structure
    CRef<class CSplitQueryBlk> m_SplitBlk;
    /// Vector of query factories, each element corresponds to a chunk
    vector< CRef<IQueryFactory> > m_QueryChunkFactories;
    /// Source of local query data
    CRef<ILocalQueryData> m_LocalQueryData;
    /// Length of the concatenated query
    size_t m_TotalQueryLength;
    /// Size of the query chunks
    size_t m_ChunkSize;
    /// Vector of CScope objects
    TScopeVector m_Scopes;
    /// Vector of masking locations
    TSeqLocInfoVector m_UserSpecifiedMasks;
    /// Vector of split queries
    TSplitQueryVector m_SplitQueriesInChunk;

    /// Auxiliary method to extract the CScope objects from the query factory
    void x_ExtractCScopesAndMasks();

    /// Compute all chunk ranges
    void x_ComputeChunkRanges();
    /// Compute query indices that correspond to each chunk
    void x_ComputeQueryIndicesForChunks();
    /// Compute query contexts that correspond to each chunk
    void x_ComputeQueryContextsForChunks();

    /// Compute the context offsets which are used to adjust the results
    void x_ComputeContextOffsetsForChunks();

    /// Compute the context offsets which are used to adjust the results for
    /// translated queries
    void x_ComputeContextOffsets_TranslatedQueries();
    /// Compute the context offsets which are used to adjust the results for
    /// non-translated queries
    void x_ComputeContextOffsets_NonTranslatedQueries();

    /// Prohibit copy constructor
    CQuerySplitter(const CQuerySplitter& rhs);
    /// Prohibit assignment operator
    CQuerySplitter& operator=(const CQuerySplitter& rhs);

    /// Declare unit test class as a friend
    friend class ::CSplitQueriesTest;
};

END_SCOPE(blast)
END_NCBI_SCOPE

/* @} */

#endif  /* ALGO_BLAST_API__SPLIT_QUERY__HPP */
