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

/** @file psiblast_impl.hpp
 * Defines implementation class for PSI-BLAST and PSI-BLAST 2 Sequences
 */

#ifndef ALGO_BLAST_API___PSIBLAST_IMPL__HPP
#define ALGO_BLAST_API___PSIBLAST_IMPL__HPP

#include <algo/blast/api/uniform_search.hpp>
#include "psiblast_subject.hpp"

/** @addtogroup AlgoBlast
 *
 * @{
 */

BEGIN_NCBI_SCOPE

/////////////////////////////////////////////////////////////////////////////
// Forward declarations

BEGIN_SCOPE(objects)
    class CPssmWithParameters;
END_SCOPE(objects)

BEGIN_SCOPE(blast)

class IQueryFactory;
class CPSIBlastOptionsHandle;
class CBlastProteinOptionsHandle;

/// Implementation class for PSI-BLAST (database search and 2 sequences).
class CPsiBlastImpl {
public:

    /// Constructor for restarting PSI-BLAST iterations with a previously
    /// generated PSSM.
    /// @param pssm same comment as in CPsiBl2Seq or CPsiBlast applies
    /// @param subject Subject sequence(s) to search [in]
    /// @param options PSI-BLAST options [in]
    CPsiBlastImpl(CRef<objects::CPssmWithParameters> pssm,
                  IPsiBlastSubject* subject,
                  CConstRef<CPSIBlastOptionsHandle> options);

    /// Constructor to handle the first iteration of PSI-BLAST when the query
    /// is a protein sequence or when the performing an object manager free
    /// BLAST 2 Sequences search
    /// @param query Protein query sequence to search (only 1 is allowed!) [in]
    /// @param subject Protein sequence to search [in]
    /// @param options Protein options [in]
    CPsiBlastImpl(CRef<IQueryFactory> query,
                  IPsiBlastSubject* subject,
                  CConstRef<CBlastProteinOptionsHandle> options);

    /// Run the PSI-BLAST engine for one iteration
    CRef<CSearchResults> Run();

    /// This method allows the same object to be reused when performing
    /// multiple iterations. Iteration state is kept in the
    /// CPsiBlastIterationState object
    /// @param pssm PSSM [in]
    void SetPssm(CConstRef<objects::CPssmWithParameters> pssm);

    /// Accessor for the most recently used PSSM
    CConstRef<objects::CPssmWithParameters> GetPssm() const;

    void SetResultType(EResultType type);

private:

    /// PSSM to be used as query
    CRef<objects::CPssmWithParameters> m_Pssm;

    /// Query sequence (either extracted from PSSM or provided in constructor)
    CRef<IQueryFactory> m_Query;

    /// PSI-BLAST subject abstraction (we don't own this resource)
    IPsiBlastSubject* m_Subject;

    /// Options to use
    CConstRef<CBlastOptionsHandle> m_OptsHandle;

    /// Holds a reference to the results
    CSearchResultSet m_Results;

    /// Specifies how the results should be produced
    EResultType m_ResultType;

    /// Prohibit copy constructor
    CPsiBlastImpl(const CPsiBlastImpl& rhs);

    /// Prohibit assignment operator
    CPsiBlastImpl& operator=(const CPsiBlastImpl& rhs);

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

#endif  /* ALGO_BLAST_API___PSIBLAST_IMPL__HPP */
