#ifndef ALGO_BLAST_API___BLASTP_KMER_OPTIONS_HPP
#define ALGO_BLAST_API___BLASTP_KMER_OPTIONS_HPP

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
 * Authors:  Tom Madden	
 *
 */

/// @file blastp_kmer_options.hpp
/// Declares the CBlastpKmerOptionsHandle class.

#include <algo/blast/api/blast_advprot_options.hpp>

/** @addtogroup AlgoBlast
 *
 * @{
 */

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(blast)

/// Handle to the KMER BLASTP options
///
/// Adapter class for KMER BLASTP comparisons.
/// Exposes an interface to allow manipulation the options that are relevant to
/// this type of search.

class NCBI_XBLAST_EXPORT CBlastpKmerOptionsHandle : 
                                public CBlastAdvancedProteinOptionsHandle
{
public:

    /// Creates object with default options set
    CBlastpKmerOptionsHandle(EAPILocality locality = CBlastOptions::eLocal);
    CBlastpKmerOptionsHandle(CRef<CBlastOptions> opt):CBlastAdvancedProteinOptionsHandle(opt) {}
    ~CBlastpKmerOptionsHandle() {}

    /******************* KMER specific options *******************/
    /// Returns threshold for Jaccard distance (range: 0-1)
    double GetThresh() const {
        return m_Thresh;
    }
    /// Sets Threshold for Jaccard distance
    /// @param thresh Jaccard threshold [in]
    void SetThresh(double thresh = 0.1) {
        m_Thresh = thresh;
    }
    /// Returns the number of hits to initiate calculation of Jaccard distance
    int GetMinHits() const {
	return m_MinHits;
    }
    /// Sets the number of hits ot initiate calculation of Jaccard distance
    /// @param hits Minimum number of LSH hits to proceed with subject [in]
    void SetMinHits(int minhits = 1) {
	m_MinHits = minhits;
    }
    /// Gets the max number of candidate matches to process with BLAST
    int GetCandidateSeqs() const {
	return m_Candidates;
    }
    /// Sets the max number of candidate matches to process with BLAST
    /// @candidates max number of target sequences to process with BLAST [in]
    void SetCandidateSeqs(int candidates = 1000) {
	m_Candidates = candidates;
    }

protected:
    /// Set the program and service name for remote blast.
    virtual void SetRemoteProgramAndService_Blast3()
    {
        m_Opts->SetRemoteProgramAndService_Blast3("kblastp", "plain");
    }
    
private:
    /// Disallow copy constructor
    CBlastpKmerOptionsHandle(const CBlastpKmerOptionsHandle& rhs);
    /// Disallow assignment operator
    CBlastpKmerOptionsHandle& operator=(const CBlastpKmerOptionsHandle& rhs);

    /// Jaccard distance cutoff
    double m_Thresh;

    /// number of hits to initiate calculation of Jaccard distance in LSH
    int m_MinHits;

    /// Number of target seqs to process with BLAST
    int m_Candidates;

    
};

END_SCOPE(blast)
END_NCBI_SCOPE


/* @} */


#endif  /* ALGO_BLAST_API___BLASTP_KMER_OPTIONS_HPP */
