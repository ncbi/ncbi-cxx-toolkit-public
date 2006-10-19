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
 * Author:  Jason Papadopoulos
 *
 */

/** @file blast_results.hpp
 * Container for data returned from a blast search (besides alignments)
 */

#ifndef ALGO_BLAST_API___BLAST_RESULTS_HPP
#define ALGO_BLAST_API___BLAST_RESULTS_HPP

#include <algo/blast/core/blast_util.h>
#include <algo/blast/core/blast_stat.h>
#include <algo/blast/core/blast_query_info.h>
#include <algo/blast/api/blast_aux.hpp>

BEGIN_NCBI_SCOPE

/** @addtogroup AlgoBlast
 *
 * @{
 */

BEGIN_SCOPE(blast)

/// Class used to return ancillary data from a blast search,
/// i.e. information that is not the list of alignment found
class CBlastAncillaryData : public CObject
{

public:

    /// constructor
    /// @param program_type Type of blast search [in]
    /// @param query_number The index of the query for which
    ///                 information will be retrieved [in]
    /// @param sbp Score block, containing Karlin parameters [in]
    /// @param query_info Structure with per-context information [in]
    CBlastAncillaryData(EBlastProgramType program_type,
                        int query_number,
                        const BlastScoreBlk *sbp,
                        const BlastQueryInfo *query_info)
        : m_UngappedKarlinBlk(0), m_GappedKarlinBlk(0), m_SearchSpace(0)
    {
        int i;
        int context_per_query = BLAST_GetNumberOfContexts(program_type);

        // find the first valid context corresponding to this query
        for (i = 0; i < context_per_query; i++) {
            BlastContextInfo *ctx = query_info->contexts + 
                                    query_number * context_per_query + i;
            if (ctx->is_valid) {
                m_SearchSpace = ctx->eff_searchsp;
                break;
            }
        }
        // fill in the Karlin blocks for that context, if they
        // are valid
        if (sbp->kbp_std) {
            Blast_KarlinBlk *kbp = 
                        sbp->kbp_std[query_number * context_per_query + i];
            if (kbp && kbp->Lambda >= 0) {
                m_UngappedKarlinBlk = Blast_KarlinBlkNew();
                Blast_KarlinBlkCopy(m_UngappedKarlinBlk, kbp);
            }
        }
        if (sbp->kbp_gap) {
            Blast_KarlinBlk *kbp = 
                        sbp->kbp_gap[query_number * context_per_query + i];
            if (kbp && kbp->Lambda >= 0) {
                m_GappedKarlinBlk = Blast_KarlinBlkNew();
                Blast_KarlinBlkCopy(m_GappedKarlinBlk, kbp);
            }
        }
    }

    /// Destructor
    ~CBlastAncillaryData()
    {
        Blast_KarlinBlkFree(m_UngappedKarlinBlk);
        Blast_KarlinBlkFree(m_GappedKarlinBlk);
    }

    /// Retrieve ungapped Karlin parameters
    const Blast_KarlinBlk * GetUngappedKarlinBlk() const
    { 
        return m_UngappedKarlinBlk; 
    }

    /// Retrieve gapped Karlin parameters
    const Blast_KarlinBlk * GetGappedKarlinBlk() const
    { 
        return m_GappedKarlinBlk; 
    }

    /// Retrieve the search space for this query sequence. If the
    /// results correspond to a blastx search, the search space will
    /// refer to protein letters
    Int8 GetSearchSpace() const
    { 
        return m_SearchSpace; 
    }

private:
    /// Ungapped Karlin parameters for one query
    Blast_KarlinBlk *m_UngappedKarlinBlk;

    /// Gapped Karlin parameters for one query
    Blast_KarlinBlk *m_GappedKarlinBlk;

    /// Search space used when calculating e-values for one query
    Int8 m_SearchSpace;
};


/// Search Results for One Query.
/// 
/// This class encapsulates all the search results and related data
/// corresponding to one of the input queries.

class NCBI_XBLAST_EXPORT CSearchResults : public CObject {
public:
    
    /// Constructor
    /// @param query List of query identifiers [in]
    /// @param align alignments for a single query sequence [in]
    /// @param errs error messages for this query sequence [in]
    CSearchResults(CConstRef<objects::CSeq_id>     query,
                   CRef<objects::CSeq_align_set>   align, 
                   const TQueryMessages          & errs,
                   CRef<CBlastAncillaryData>       ancillary_data)
        : m_QueryId(query), m_Alignment(align), 
          m_Errors(errs), m_AncillaryData(ancillary_data)
    {
    }
    
    /// Accessor for the Seq-align results
    CConstRef<objects::CSeq_align_set> GetSeqAlign() const
    {
        return m_Alignment;
    }

    /// Return true if there are any alignments for this query
    bool HasAlignments() const;

    /// Accessor for the query's sequence identifier
    CConstRef<objects::CSeq_id> GetSeqId() const;
    
    /// Accessor for the query's search ancillary
    CRef<CBlastAncillaryData> GetAncillaryData() const
    {
        return m_AncillaryData;
    }
    
    /// Accessor for the error/warning messsages for this query
    /// @param min_severity minimum severity to report errors [in]
    TQueryMessages GetErrors(int min_severity = eBlastSevError) const;

    /// Retrieve the query regions which were masked by BLAST
    /// @param flt_query_regions the return value [in|out]
    void GetMaskedQueryRegions(TMaskedQueryRegions& flt_query_regions) const;

    /// Mutator for the masked query regions, intended to be used by internal
    /// BLAST APIs to populate this object
    /// @param flt_query_regions the input value [in]
    void SetMaskedQueryRegions(TMaskedQueryRegions& flt_query_regions);
    
private:
    /// this query's id
    CConstRef<objects::CSeq_id> m_QueryId;
    
    /// alignments for this query
    CRef<objects::CSeq_align_set> m_Alignment;
    
    /// error/warning messages for this query
    TQueryMessages m_Errors;

    /// this query's masked regions
    TMaskedQueryRegions m_Masks;

    /// non-alignment ancillary data for this query
    CRef<CBlastAncillaryData> m_AncillaryData;
};


/// Search Results for All Queries.
/// 
/// This class encapsulates all of the search results and related data
/// from a search.

class NCBI_XBLAST_EXPORT CSearchResultSet {
public:
    /// List of query ids.
    typedef vector< CConstRef<objects::CSeq_id> > TQueryIdVector;
    
    /// size_type type definition
    typedef vector< CRef<CSearchResults> >::size_type size_type;
    
    /// size_type type definition
    typedef vector< CRef<CBlastAncillaryData> > TAncillaryVector;
    
    /// Default constructor
    CSearchResultSet() {}
    
    /// Parametrized constructor
    /// @param aligns vector of all queries' alignments [in]
    /// @param msg_vec vector of all queries' messages [in]
    CSearchResultSet(TSeqAlignVector aligns,
                     TSearchMessages msg_vec);
    
    /// Parametrized constructor
    /// @param ids vector of all queries' ids [in]
    /// @param aligns vector of all queries' alignments [in]
    /// @param msg_vec vector of all queries' messages [in]
    /// @param ancillary_data vector of per-query search ancillary data [in]
    CSearchResultSet(TQueryIdVector  ids,
                     TSeqAlignVector aligns,
                     TSearchMessages msg_vec,
                     TAncillaryVector  ancillary_data = 
                     TAncillaryVector());
    
    /// Allow array-like access with integer indices to CSearchResults 
    /// contained by this object
    /// @param i query sequence index [in]
    CSearchResults & operator[](size_type i)
    {
        return *m_Results[i];
    }
    
    /// Allow array-like access with integer indices to const CSearchResults 
    /// contained by this object
    /// @param i query sequence index [in]
    const CSearchResults & operator[](size_type i) const
    {
        return *m_Results[i];
    }
    
    /// Allow array-like access with CSeq_id indices to CSearchResults 
    /// contained by this object
    /// @param ident query sequence identifier [in]
    CRef<CSearchResults> operator[](const objects::CSeq_id & ident);
    
    /// Allow array-like access with CSeq_id indices to const CSearchResults 
    /// contained by this object
    /// @param ident query sequence identifier [in]
    CConstRef<CSearchResults> operator[](const objects::CSeq_id & ident) const;
    
    /// Return the number of results contained by this object
    size_type GetNumResults() const
    {
        return m_Results.size();
    }
    
    /// Add results to this object, intended to be used by internal
    /// BLAST APIs to populate this object
    /// @param result results for a given query [in]
    void AddResult(CRef<CSearchResults> result)
    {
        m_Results.push_back(result);
    }
private:    
    /// Initialize the result set.
    void x_Init(vector< CConstRef<objects::CSeq_id> > queries,
                TSeqAlignVector                       aligns,
                TSearchMessages                       msg_vec,
                TAncillaryVector                      ancillary_data);
    
    /// Vector of results.
    vector< CRef<CSearchResults> > m_Results;
};

END_SCOPE(blast)
END_NCBI_SCOPE
/* @} */

/*
* ===========================================================================
*
* $Log$
* Revision 1.2  2006/10/19 17:45:36  camacho
* minor changes
*
* Revision 1.1  2006/10/19 15:20:55  papadopo
* Classes for retrieving results from blast searches
*
*
* ===========================================================================
*/


#endif /* ALGO_BLAST_API___BLAST_RESULTS_HPP */
