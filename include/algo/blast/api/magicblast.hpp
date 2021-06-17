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
 * Author:  Greg Boratyn
 *
 */

/// @file magicblast.hpp
/// Declares CMagicBlast, the C++ API for the BLAST RNA-Seq mapping engine.

#ifndef ALGO_BLAST_API___MAGICBLAST__HPP
#define ALGO_BLAST_API___MAGICBLAST__HPP

#include <algo/blast/core/spliced_hits.h>
#include <algo/blast/api/setup_factory.hpp>
#include <algo/blast/api/query_data.hpp>
#include <algo/blast/api/local_db_adapter.hpp>
#include <algo/blast/api/blast_results.hpp>
#include <algo/blast/api/magicblast_options.hpp>
#include <algo/blast/api/prelim_stage.hpp>

/** @addtogroup AlgoBlast
 *
 * @{
 */

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(blast)

// Forward declarations
class IQueryFactory;
class CMagicBlastResultSet;

/// BLAST RNA-Seq mapper
class NCBI_XBLAST_EXPORT CMagicBlast : public CObject, public CThreadable
{
public:

public:
    /// Constructor to map short reads as queries to a genome as BLAST database
    /// @param query_factory
    ///     Short reads sequence to map [in]
    /// @param blastdb
    ///     Adapter to the BLAST database [in]
    /// @param options
    ///     MAGIC-BLAST options [in]
    CMagicBlast(CRef<IQueryFactory> query_factory,
                CRef<CLocalDbAdapter> blastdb,
                CRef<CMagicBlastOptionsHandle> options);

    /// Destructor
    ~CMagicBlast() {}

    /// Run the RNA-Seq mapping
    CRef<CSeq_align_set> Run(void);

    CRef<CMagicBlastResultSet> RunEx(void);

    TSearchMessages GetSearchMessages(void) const
    {return m_Messages;}


protected:
    /// Prohibit copy constructor
    CMagicBlast(const CMagicBlast& rhs);
    /// Prohibit assignment operator
    CMagicBlast& operator=(const CMagicBlast& rhs);

    /// Perform sanity checks on input arguments
    void x_Validate(void);

    int x_Run(void);


    CRef<CSeq_align_set> x_BuildSeqAlignSet(
                                      const BlastMappingResults* results);

    CRef<CMagicBlastResultSet> x_BuildResultSet(
                                      const BlastMappingResults* results);


    /// Create results
    static CRef<CSeq_align_set> x_CreateSeqAlignSet(const HSPChain* results,
                                           CRef<ILocalQueryData> qdata,
                                           CRef<IBlastSeqInfoSrc> seqinfo_src,
                                           const BlastQueryInfo* query_info);

private:
    /// Queries
    CRef<IQueryFactory> m_Queries;

    /// Reference to a BLAST subject/database object
    CRef<CLocalDbAdapter> m_LocalDbAdapter;

    /// Options to configure the search
    CRef<CBlastOptions> m_Options;

    /// Object that runs BLAST search
    CRef<CBlastPrelimSearch> m_PrelimSearch;

    /// Internal data strctures
    CRef<SInternalData> m_InternalData;

    /// Warning and error messages
    TSearchMessages m_Messages;
};


/// Magic-BLAST results for a single query/read or a pair of reads
class NCBI_XBLAST_EXPORT CMagicBlastResults : public CObject
{
public:

    /// Information flags about mapping results
    typedef enum {
        /// Read is unaligned
        fUnaligned = 1,

        /// Read did not pass quality filtering
        fFiltered = 1 << 1
    } EResultsInfo;

    typedef int TResultsInfo;

    /// Ordering of alignments
    typedef enum {
        eFwRevFirst = 0,
        eRevFwFirst
    } EOrdering;


    /// Constructor for a pair
    CMagicBlastResults(CConstRef<CSeq_id> query_id, CConstRef<CSeq_id> mate_id,
                       CRef<CSeq_align_set> aligns,
                       const TMaskedQueryRegions* query_mask = NULL,
                       const TMaskedQueryRegions* mate_mask = NULL,
                       int query_length = 0,
                       int mate_length = 0);


    /// Constructor for a single read
    CMagicBlastResults(CConstRef<CSeq_id> query_id,
                       CRef<CSeq_align_set> aligns,
                       const TMaskedQueryRegions* query_mask = NULL,
                       int query_length = 0);

    /// Get alignments
    CConstRef<CSeq_align_set> GetSeqAlign(void) const {return m_Aligns;}

    /// Get non-const alignments
    CRef<CSeq_align_set> SetSeqAlign(void) {return m_Aligns;}

    /// Are alignments computed for paired reads
    bool IsPaired(void) const {return m_Paired;}

    /// Are an aligned pair concordant?
    bool IsConcordant(void) const {return m_Concordant;}

    /// Get alignment flags for the query
    TResultsInfo GetFirstInfo(void) const {return m_FirstInfo;}

    /// Get alignment flags for the mate
    TResultsInfo GetLastInfo(void) const {return m_LastInfo;}

    /// Get query sequence id
    const CSeq_id& GetQueryId(void) const {return *m_QueryId;}

    /// Get sequence id of the first segment of a paired read
    const CSeq_id& GetFirstId(void) const {return GetQueryId();}

    /// Get sequence id of the last sequence of a paired read
    const CSeq_id& GetLastId(void) const {return *m_MateId;}

    /// Is the query aligned
    bool FirstAligned(void) const {return (m_FirstInfo & fUnaligned) != 0;}

    /// Is the mate aligned
    bool LastAligned(void) const {return (m_LastInfo & fUnaligned) != 0;}

    /// Sort alignments by selected criteria (pair configuration)
    void SortAlignments(EOrdering order);

private:
    void x_SetInfo(int first_length,
                   const TMaskedQueryRegions* first_masks,
                   int last_length = 0,
                   const TMaskedQueryRegions* last_masks = NULL);

private:
    /// Query id
    CConstRef<CSeq_id> m_QueryId;

    /// Mate id if results are for paired reads
    CConstRef<CSeq_id> m_MateId;

    /// Alignments for a single or a pair of reads
    CRef<CSeq_align_set> m_Aligns;

    /// True if results are for paired reads
    bool m_Paired;

    /// True if results are concordant pair
    bool m_Concordant;

    /// Alignment flags for the query
    TResultsInfo m_FirstInfo;

    /// Alignment flags for the mate
    TResultsInfo m_LastInfo;
};


/// Results of Magic-BLAST mapping
class NCBI_XBLAST_EXPORT CMagicBlastResultSet : public CObject
{
public:

    /// data type contained by this container
    typedef CRef<CMagicBlastResults> value_type;

    /// size_type type definition
    typedef vector<value_type>::size_type size_type;

    /// const_iterator type definition
    typedef vector<value_type>::const_iterator const_iterator;

    /// iterator type definition
    typedef vector<value_type>::iterator iterator;

    /// Create an empty results set
    CMagicBlastResultSet(void) {}

    /// Get all results as a single Seq-align-set object
    /// @param no_discordant Report only paires aligned concordantly [in]
    CRef<CSeq_align_set> GetFlatResults(bool no_discordant = false);

    /// Get number of results, provided to facilitate STL-style iteration
    size_type size() const {return m_Results.size();}

    /// Is the container empty
    bool empty() const {return size() == 0;}

    /// Returns const iteartor to the beginning of the container,
    /// provided to facilitate STL-style iteartion
    const_iterator begin() const {return m_Results.begin();}

    /// Returns const iterator to the end of the container
    const_iterator end() const {return m_Results.end();}

    /// Returns iterator to the beginning of the container
    iterator begin() {return m_Results.begin();}

    /// Returns iterator to the end of the container
    iterator end() {return m_Results.end();}

    /// Clear all results
    void clear() {m_Results.clear();}

    /// Reserve memory for a number of result elemetns
    void reserve(size_t num) {m_Results.reserve(num);}

    /// Add results to the end of the container
    void push_back(CMagicBlastResultSet::value_type& element)
    {m_Results.push_back(element);}

private:
    CMagicBlastResultSet(const CMagicBlastResultSet&);
    CMagicBlastResultSet& operator=(const CMagicBlastResultSet&);

    vector< CRef<CMagicBlastResults> > m_Results;
};


END_SCOPE(blast)
END_NCBI_SCOPE

/* @} */

#endif  /* ALGO_BLAST_API___MAGICBLAST__HPP */
