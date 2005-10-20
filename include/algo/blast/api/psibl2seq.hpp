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
 * Author:  Christiam Camacho
 *
 */

/// @file psibl2seq.hpp
/// Declares CPsiBl2Seq, the C++ API for the PSI-BLAST 2 Sequences engine.

#ifndef ALGO_BLAST_API___PSIBL2SEQ__HPP
#define ALGO_BLAST_API___PSIBL2SEQ__HPP

#include <algo/blast/api/uniform_search.hpp>
#include <algo/blast/api/psiblast_options.hpp>

/** @addtogroup AlgoBlast
 *
 * @{
 */

BEGIN_NCBI_SCOPE

BEGIN_SCOPE(objects)
    class CPssmWithParameters;
END_SCOPE(objects)

BEGIN_SCOPE(blast)

// Forward declarations
class IQueryFactory;

/// Runs the PSI-BLAST algorithm between 2 sequences.

class NCBI_XBLAST_EXPORT CPsiBl2Seq : public CObject
{
public:
    /// Constructor to compare a PSSM against protein sequences
    ///
    /// @param pssm PSSM to use as query. This must contain the query sequence
    /// which represents the master sequence for the PSSM. PSSM data might be
    /// provided as scores or as frequency ratios, in which case the PSSM 
    /// engine will be invoked to convert them to scores (and save them as a
    /// effect). If both the scores and frequency ratios are provided, the 
    /// scores are given priority and are used in the search. [in|out]
    /// @todo how should scaled PSSM scores be handled?
    ///
    /// @param subject Subject sequence(s) to search [in]
    ///
    /// @param options PSI-BLAST options [in]
    CPsiBl2Seq(CRef<objects::CPssmWithParameters> pssm,
               CRef<IQueryFactory> subject,
               CConstRef<CPSIBlastOptionsHandle> options);

    /// Constructor to compare two protein sequences in an object manager-free
    /// manner.
    /// @query Protein query sequence to search (only 1 is allowed!) [in]
    /// @subject Protein nucleotide sequence to search [in]
    /// @options Protein options [in]
    CPsiBl2Seq(CRef<IQueryFactory> query,
               CRef<IQueryFactory> subject,
               CConstRef<CBlastProteinOptionsHandle> options);

    /// Run the PSI-BLAST 2 Sequences engine
    CRef<CSearchResults> Run();

private:
    /// PSSM to be used as query
    CRef<objects::CPssmWithParameters> m_Pssm; 

    /// Query sequence (either extracted from PSSM or provided in constructor
    CRef<IQueryFactory> m_Query;

    /// Subject sequences
    CRef<IQueryFactory> m_Subject;  

    /// Options to use 
    CConstRef<CBlastOptionsHandle> m_OptsHandle;

    /// Holds a reference to the results
    CSearchResultSet m_Results;

    /// Prohibit copy constructor
    CPsiBl2Seq(const CPsiBl2Seq& rhs);

    /// Prohibit assignment operator
    CPsiBl2Seq& operator=(const CPsiBl2Seq& rhs);

    /// Perform sanity checks on input parameters
    void x_Validate();

    /// Computes the PSSM scores in case these are not available in the PSSM
    void x_CreatePssmScoresFromFrequencyRatios();

    /// Auxiliary function to get the query sequence data from the ASN.1 PSSM
    /// Post-condition: (m_Query.Empty() == false)
    void x_ExtractQueryFromPssm();
};

END_SCOPE(blast)
END_NCBI_SCOPE

/* @} */

/*
* ===========================================================================
*
* $Log$
* Revision 1.4  2005/10/20 00:08:19  camacho
* + protein bl2seq interface (in progress)
*
* Revision 1.3  2005/10/14 12:18:58  camacho
* Make CPsiBl2Seq use CBlastTracebackSearch
*
* Revision 1.2  2005/10/12 13:52:45  camacho
* Doxygen fixes
*
* Revision 1.1  2005/10/11 12:19:53  camacho
* Initial revision
*
* ===========================================================================
*/

#endif  /* ALGO_BLAST_API___PSIBL2SEQ__HPP */
