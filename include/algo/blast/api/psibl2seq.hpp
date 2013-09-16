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
#include <algo/blast/api/local_db_adapter.hpp>

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

/// Runs a single iteration of the PSI-BLAST algorithm between 2 sequences.
class NCBI_XBLAST_EXPORT CPsiBl2Seq : public CObject
{
public:
    /// Constructor to compare a PSSM against protein sequences
    /// @param pssm 
    ///     PSSM to use as query. This must contain the query sequence which
    ///     represents the master sequence for the PSSM. PSSM data might be
    ///     provided as scores or as frequency ratios, in which case the PSSM 
    ///     engine will be invoked to convert them to scores (and save them as a
    ///     effect). If both the scores and frequency ratios are provided, the 
    ///     scores are given priority and are used in the search. [in|out]
    ///     @todo how should scaled PSSM scores be handled?
    /// @param subject 
    ///     Subject sequence(s) to search [in]
    /// @param options 
    ///     PSI-BLAST options [in]
    CPsiBl2Seq(CRef<objects::CPssmWithParameters> pssm,
               CRef<IQueryFactory> subject,
               CConstRef<CPSIBlastOptionsHandle> options);

    /// Constructor to compare a PSSM against protein sequences
    /// @param pssm 
    ///     PSSM to use as query. This must contain the query sequence which
    ///     represents the master sequence for the PSSM. PSSM data might be
    ///     provided as scores or as frequency ratios, in which case the PSSM 
    ///     engine will be invoked to convert them to scores (and save them as a
    ///     effect). If both the scores and frequency ratios are provided, the 
    ///     scores are given priority and are used in the search. [in|out]
    ///     @todo how should scaled PSSM scores be handled?
    /// @param subject 
    ///     Subject sequence(s) to search [in]
    /// @param options 
    ///     PSI-BLAST options [in]
    CPsiBl2Seq(CRef<objects::CPssmWithParameters> pssm,
               CRef<CLocalDbAdapter> subject,
               CConstRef<CPSIBlastOptionsHandle> options);


    /// Constructor to compare protein sequences in an object manager-free
    /// manner.
    /// @param query 
    ///     Protein query sequence to search (only 1 is allowed!) [in]
    /// @param subject 
    ///     Protein sequence(s) to search [in]
    /// @param options 
    ///     Protein options [in]
    CPsiBl2Seq(CRef<IQueryFactory> query,
               CRef<IQueryFactory> subject,
               CConstRef<CBlastProteinOptionsHandle> options);

    /// Destructor
    ~CPsiBl2Seq();

    /// Run the PSI-BLAST 2 Sequences engine
    CRef<CSearchResultSet> Run();

private:

    /// Reference to a BLAST subject/database object
    CRef<CLocalDbAdapter> m_Subject;

    /// Implementation class
    class CPsiBlastImpl* m_Impl;

    /// Auxiliary method to initialize the subject
    /// @param subject query factory describing the subject sequence(s) [in]
    /// @param options PSI-BLAST options [in]
    /// @throws CBlastException if options is empty
    /// @post the m_Subject member will be initialized
    void x_InitSubject(CRef<IQueryFactory> subject,
                       const CBlastOptionsHandle* options);

    /// Prohibit copy constructor
    CPsiBl2Seq(const CPsiBl2Seq& rhs);

    /// Prohibit assignment operator
    CPsiBl2Seq& operator=(const CPsiBl2Seq& rhs);

};

END_SCOPE(blast)
END_NCBI_SCOPE

/* @} */

#endif  /* ALGO_BLAST_API___PSIBL2SEQ__HPP */
