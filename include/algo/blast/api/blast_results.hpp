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
 * Definition of classes which constitute the results of running a BLAST
 * search
 */

#ifndef ALGO_BLAST_API___BLAST_RESULTS_HPP
#define ALGO_BLAST_API___BLAST_RESULTS_HPP

#include <algo/blast/core/blast_stat.h>
#include <algo/blast/api/blast_aux.hpp>

/** @addtogroup AlgoBlast
 *
 * @{
 */

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(blast)

/// Class used to return ancillary data from a blast search,
/// i.e. information that is not the list of alignment found
class NCBI_XBLAST_EXPORT CBlastAncillaryData : public CObject
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
                        const BlastQueryInfo *query_info);

    /** Parametrized constructor taking pairs of values for ungapped and gapped
     * Karlin-Altschul parameters as well as the effective search space
     * @param lambda Pair of ungapped and gapped lambda (in that order) [in]
     * @param k Pair of ungapped and gapped k (in that order) [in]
     * @param h Pair of ungapped and gapped h (in that order) [in]
     * @param effective_search_space effective search space [in]
     * @param is_psiblast true if the statistical parameters are for PSI-BLAST
     * [in]
     */
    CBlastAncillaryData(pair<double, double> lambda,
                        pair<double, double> k,
                        pair<double, double> h,
                        Int8 effective_search_space,
                        bool is_psiblast = false);

    /// Destructor
    ~CBlastAncillaryData();

    /// Copy-constructor
    CBlastAncillaryData(const CBlastAncillaryData& rhs) {
        do_copy(rhs);
    }

    /// Assignment operator
    CBlastAncillaryData& operator=(const CBlastAncillaryData& rhs) {
        do_copy(rhs);
        return *this;
    }

    /// Retrieve ungapped Karlin parameters
    const Blast_KarlinBlk * GetUngappedKarlinBlk() const { 
        return m_UngappedKarlinBlk; 
    }

    /// Retrieve gapped Karlin parameters
    const Blast_KarlinBlk * GetGappedKarlinBlk() const { 
        return m_GappedKarlinBlk; 
    }

    /// Retrieve PSI-BLAST ungapped Karlin parameters
    const Blast_KarlinBlk * GetPsiUngappedKarlinBlk() const { 
        return m_PsiUngappedKarlinBlk; 
    }

    /// Retrieve PSI-BLAST gapped Karlin parameters
    const Blast_KarlinBlk * GetPsiGappedKarlinBlk() const { 
        return m_PsiGappedKarlinBlk; 
    }
    /// Retrieve the search space for this query sequence. If the
    /// results correspond to a blastx search, the search space will
    /// refer to protein letters
    Int8 GetSearchSpace() const { 
        return m_SearchSpace; 
    }
    /// Retrieve the length adjustment for boundary conditions
    Int8 GetLengthAdjustment() const { 
        return m_LengthAdjustment; 
    }
private:
    /// Ungapped Karlin parameters for one query
    Blast_KarlinBlk *m_UngappedKarlinBlk;

    /// Gapped Karlin parameters for one query
    Blast_KarlinBlk *m_GappedKarlinBlk;

    /// PSI-BLAST ungapped Karlin parameters for one query (if applicable)
    Blast_KarlinBlk *m_PsiUngappedKarlinBlk;

    /// PSI-BLAST gapped Karlin parameters for one query (if applicable)
    Blast_KarlinBlk *m_PsiGappedKarlinBlk;

    /// Search space used when calculating e-values for one query

    Int8 m_SearchSpace;

    /// Length adjustment for boundary conditions
    Int8 m_LengthAdjustment;

    /// Workhorse for copy constructor and assignment operator
    /// @param other object to copy [in]
    void do_copy(const CBlastAncillaryData& other);
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
    /// @param ancillary_data Miscellaneous output from the blast engine [in]
    /// @param query_masks Mask locations for this query [in]
    /// @param rid RID (if applicable, else empty string) [in]
    CSearchResults(CConstRef<objects::CSeq_id>     query,
                   CRef<objects::CSeq_align_set>   align, 
                   const TQueryMessages          & errs,
                   CRef<CBlastAncillaryData>       ancillary_data,
                   const TMaskedQueryRegions     * query_masks = NULL,
                   const string                  & rid = kEmptyStr,
                   const SPHIQueryInfo           * phi_query_info = NULL);

    /// Our destructor
    ~CSearchResults();
        
    /// Sets the RID for these results
    /// @param rid RID to set [in]
    void SetRID(const string& rid) { m_RID.assign(rid); }

    /// Returns the RID for these results (if applicable), otherwise returns an
    /// empty string
    string GetRID() const { return m_RID; }
    
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

    /// Returns true if there are errors among the results for this object
    bool HasErrors() const;
    /// Returns true if there are warnings among the results for this object
    bool HasWarnings() const;

    /// Retrieve a string with the query identifier followed by the errors
    /// produced, returns a empty string if HasErrors() returns false.
    string GetErrorStrings() const;
    /// Retrieve a string with the query identifier followed by the warnings
    /// produced, returns a empty string if HasWarnings() returns false.
    string GetWarningStrings() const;

    /// Retrieve the query regions which were masked by BLAST
    /// @param flt_query_regions the return value [in|out]
    void GetMaskedQueryRegions(TMaskedQueryRegions& flt_query_regions) const;

    /// Mutator for the masked query regions, intended to be used by internal
    /// BLAST APIs to populate this object
    /// @param flt_query_regions the input value [in]
    void SetMaskedQueryRegions(const TMaskedQueryRegions& flt_query_regions);
    
    /// Retrieve the masked locations for the subject sequences in the
    /// contained alignment
    /// @param subj_masks masked locations [out]
    void GetSubjectMasks(TSeqLocInfoVector& subj_masks) const;

    /// Set the masked locations for the subject sequences in the
    /// contained alignment
    /// @param subj_masks masked locations [in]
    void SetSubjectMasks(const TSeqLocInfoVector& subj_masks);

    /// Retrieves PHI-BLAST information about pattern on query.
    const SPHIQueryInfo * GetPhiQueryInfo() const {
         return m_PhiQueryInfo;
    }

protected:
    /// this query's id
    CConstRef<objects::CSeq_id> m_QueryId;
    
    /// alignments for this query
    CRef<objects::CSeq_align_set> m_Alignment;
    
    /// error/warning messages for this query
    TQueryMessages m_Errors;

    /// this query's masked regions
    TMaskedQueryRegions m_Masks;

    /// the matching subjects masks
    TSeqLocInfoVector m_SubjectMasks;

    /// non-alignment ancillary data for this query
    CRef<CBlastAncillaryData> m_AncillaryData;

    /// The RID, if applicable (otherwise it's empty)
    string m_RID;

    /// PHI-BLAST information.
    SPHIQueryInfo *m_PhiQueryInfo;

    /// Prohibit copy constructor
    CSearchResults(const CSearchResults& rhs);
    /// Prohibit assignment operator
    CSearchResults& operator=(const CSearchResults& rhs);
};


/// Search Results for All Queries.
/// 
/// This class encapsulates all of the search results and related data
/// from a search, it supports BLAST database and Bl2Seq searches and provides
/// a convenient way of accessing the results from BLAST.
///
/// @note When representing BLAST database results, there are
/// CSearchResultSet::NumQueries() objects of type CSearchResultSet::value_type 
/// in this object. When representing Bl2Seq results, there are
/// (CSearchResultSet::NumQueries() * number of subjects) objects of type
/// CSearchResultSet::value_type in this object.

class NCBI_XBLAST_EXPORT CSearchResultSet : public CObject {
public:
    /// data type contained by this container
    typedef CRef<CSearchResults> value_type;

    /// List of query ids.
    typedef vector< CConstRef<objects::CSeq_id> > TQueryIdVector;
    
    /// size_type type definition
    typedef vector<value_type>::size_type size_type;
    
    /// typedef for a vector of CRef<CBlastAncillaryData>
    typedef vector< CRef<CBlastAncillaryData> > TAncillaryVector;

    /// const_iterator type definition
    typedef vector<value_type>::const_iterator const_iterator;

    /// iterator type definition
    typedef vector<value_type>::iterator iterator;

    /// Simplest constructor
    CSearchResultSet(EResultType res_type = eDatabaseSearch);

    /// Parametrized constructor
    /// @param aligns vector of all queries' alignments [in]
    /// @param msg_vec vector of all queries' messages [in]
    /// @param res_type result type stored in this object [in]
    CSearchResultSet(TSeqAlignVector aligns,
                     TSearchMessages msg_vec,
                     EResultType res_type = eDatabaseSearch);
    
    /// Parametrized constructor
    /// @param ids vector of all queries' ids [in]
    /// @param aligns vector of all queries' alignments [in]
    /// @param msg_vec vector of all queries' messages [in]
    /// @param ancillary_data vector of per-query search ancillary data [in]
    /// @param masks Mask locations for this query [in]
    /// @param res_type result type stored in this object [in]
    /// @note this constructor assumes that the ids, msg_vec, and 
    /// ancillary_data vectors are of the SAME size as the aligns vector. The
    /// masks vector can be of the same size as aligns or have as many elements
    /// as there were queries in the search and they will be adjusted as
    /// necessary.
    CSearchResultSet(TQueryIdVector  ids,
                     TSeqAlignVector aligns,
                     TSearchMessages msg_vec,
                     TAncillaryVector  ancillary_data = 
                     TAncillaryVector(),
                     const TSeqLocInfoVector* masks = NULL,
                     EResultType res_type = eDatabaseSearch,
                     const SPHIQueryInfo* phi_query_info = NULL);

    /// Allow array-like access with integer indices to CSearchResults 
    /// contained by this object
    /// @param i query sequence index if result type is eDatabaseSearch,
    /// otherwise it's the query-subject index [in]
    CSearchResults & operator[](size_type i) {
        return *m_Results[i];
    }
    
    /// Allow array-like access with integer indices to const CSearchResults 
    /// contained by this object
    /// @param i query sequence index if result type is eDatabaseSearch,
    /// otherwise it's the query-subject index [in]
    const CSearchResults & operator[](size_type i) const {
        return *m_Results[i];
    }

    /// Retrieve results for a query-subject pair
    /// contained by this object
    /// @param qi query sequence index [in]
    /// @param si subject sequence index [in]
    /// @note it only works for results of type eSequenceComparison
    CSearchResults & GetResults(size_type qi, size_type si);

    /// Retrieve results for a query-subject pair
    /// @param qi query sequence index [in]
    /// @param si subject sequence index [in]
    /// @note it only works for results of type eSequenceComparison
    const CSearchResults & GetResults(size_type qi, size_type si) const;
    
    /// Allow array-like access with CSeq_id indices to CSearchResults 
    /// contained by this object
    /// @param ident query sequence identifier [in]
    /// @note it only works for results of type eDatabaseSearch
    CRef<CSearchResults> operator[](const objects::CSeq_id & ident);
    
    /// Allow array-like access with CSeq_id indices to const CSearchResults 
    /// contained by this object
    /// @param ident query sequence identifier [in]
    /// @note it only works for results of type eDatabaseSearch
    CConstRef<CSearchResults> operator[](const objects::CSeq_id & ident) const;
    
    /// Return the number of results contained by this object
    /// @note this returns the number of queries for results of type 
    /// eDatabaseSearch and (number of queries * number of subjects) for results
    /// of type eSequenceComparison
    size_type GetNumResults() const
    {
        return m_Results.size();
    }

    /// Return the number of unique query ID's represented by this object
    size_type GetNumQueries()
    {
        return m_NumQueries;
    }

    /// Sets the filtered query regions. If results are of type
    /// eSequenceComparison, the masks can be one for each query and they will
    /// be duplicated as necessary to meet this class' pre-conditions.
    void SetFilteredQueryRegions(const TSeqLocInfoVector& masks);

    /// Identical to GetNumResults, provided to facilitate STL-style iteration
    /// @sa note in GetNumResults
    size_type size() const { return GetNumResults(); }

    /// Returns const_iterator to beginning of container, provided to
    /// facilitate STL-style iteration
    const_iterator begin() const { return m_Results.begin(); }

    /// Returns const_iterator to end of container, provided to
    /// facilitate STL-style iteration
    const_iterator end() const { return m_Results.end(); }

    /// Returns iterator to beginning of container, provided to
    /// facilitate STL-style iteration
    iterator begin() { return m_Results.begin(); }

    /// Returns iterator to end of container, provided to
    /// facilitate STL-style iteration
    iterator end() { return m_Results.end(); }

    /// Clears the contents of this object
    void clear() {
        m_NumQueries = 0;
        m_Results.clear();
    }

    /// Add a value to the back of this container
    /// @param element element to add [in]
    void push_back(value_type& element);

    /// Get the type of results contained in this object
    EResultType GetResultType() const { return m_ResultType; }
    
    /// Sets the RID for these results
    /// @param rid RID to set [in]
    void SetRID(const string& rid);

private:    
    /// Initialize the result set.
    void x_Init(TQueryIdVector& queries,
                TSeqAlignVector                       aligns,
                TSearchMessages                       msg_vec,
                TAncillaryVector                      ancillary_data,
                const TSeqLocInfoVector*              query_masks,
                const SPHIQueryInfo*                  phi_query_info = NULL);
    
    /// Type of results stored in this object
    EResultType m_ResultType;

    /// Number of queries
    size_type m_NumQueries;

    /// Vector of results.
    vector< CRef<CSearchResults> > m_Results;

    /// True if this object contains PHI-BLAST results
    bool m_IsPhiBlast;
};

END_SCOPE(blast)
END_NCBI_SCOPE

/* @} */

#endif /* ALGO_BLAST_API___BLAST_RESULTS_HPP */
