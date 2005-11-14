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

/** @file psiblast_aux_priv.hpp
 * Auxiliary functions for PSI-BLAST
 */

#ifndef ALGO_BLAST_API___PSIBLAST_AUX_PRIV__HPP
#define ALGO_BLAST_API___PSIBLAST_AUX_PRIV__HPP

#include <corelib/ncbiobj.hpp>
#include <objects/seqalign/Dense_seg.hpp>

/** @addtogroup AlgoBlast
 *
 * @{
 */

struct BlastScoreBlk;
struct PSIBlastOptions;

BEGIN_NCBI_SCOPE

/////////////////////////////////////////////////////////////////////////////
// Forward declarations

template <class T> class CNcbiMatrix;

BEGIN_SCOPE(objects)
    class CSeq_id;
    class CSeq_align_set;
    class CPssmWithParameters;
END_SCOPE(objects)

BEGIN_SCOPE(blast)

class CBlastOptions;
class CBlastOptionsHandle;
class IQueryFactory;

/////////////////////////////////////////////////////////////////////////////
// Function prototypes/Class definitions

/** Setup CORE BLAST score block structure with data from the scoremat PSSM.
 * @param score_blk BlastScoreBlk structure to set up [in|out]
 * @param pssm scoremat PSSM [in]
 */
void PsiBlastSetupScoreBlock(BlastScoreBlk* score_blk,
                             CConstRef<objects::CPssmWithParameters> pssm);

/** Given a PSSM with frequency ratios and options, invoke the PSSM engine to
 * compute the scores.
 * @param pssm object containing the PSSM's frequency ratios [in|out]
 * @param opts PSSM engine options [in]
 */
void PsiBlastComputePssmScores(CRef<objects::CPssmWithParameters> pssm,
                               const CBlastOptions& opts);

/// Returns the lowest score from the list of scores in CDense_seg::TScores
/// @param scores list of scores [in]
double GetLowestEvalue(const objects::CDense_seg::TScores& scores);

/** Auxiliary class to retrieve sequence identifiers its position in the
 * alignment which are below the inclusion evalue threshold.
 */
class CPsiBlastAlignmentProcessor {
public:
    /// Typedef to identify an interesting hit in an alignment by its index in
    /// the CSeq_align_set::Tdata and its actual Seq-id
    typedef pair<unsigned int, CRef<objects::CSeq_id> > THitId;

    /// Container of THitId
    typedef vector<THitId> THitIdentifiers;

    /// Extract all the THitId which have evalues below the inclusion threshold
    /// @param alignments 
    ///     alignments corresponding to one query sequence [in]
    /// @param evalue_inclusion_threshold
    ///     All hits in the above alignment which have evalues below this
    ///     parameter will be included in the return value [in]
    /// @param output
    ///     Return value of this method [out]
    void operator()(const objects::CSeq_align_set& alignments,
                    double evalue_inclusion_threshold,
                    THitIdentifiers& output);
};

/// Auxialiry class containing static methods to validate PSI-BLAST search
/// components
class CPsiBlastValidate {
public:

    /** Perform validation on the PSSM
     * @param pssm PSSM as specified in scoremat.asn [in]
     * @param require_scores Set to true if scores MUST be present (otherwise,
     * either scores or frequency ratios are acceptable) [in]
     * @throws CBlastException on failure when validating data
     */
    static void
    Pssm(const objects::CPssmWithParameters& pssm,
         bool require_scores = false);

    /// Enumeration to specify the different uses of the query factory
    enum EQueryFactoryType { eQFT_Query, eQFT_Subject };

    /// Function to perform sanity checks on the query factory
    static void
    QueryFactory(CRef<IQueryFactory> query_factory, 
                 const CBlastOptionsHandle& opts_handle, 
                 EQueryFactoryType query_factory_type = eQFT_Query);
};

/// Auxiliary class to convert data encoded in the PSSM to CNcbiMatrix
class CScorematPssmConverter 
{
public:
    /// Returns matrix of BLASTAA_SIZE by query size (dimensions are opposite of
    /// what is stored in the BlastScoreBlk) containing scores
    /// Throws std::runtime_error if scores are not available
    static CNcbiMatrix<int>*
    GetScores(CConstRef<objects::CPssmWithParameters> pssm);

    /// Returns matrix of BLASTAA_SIZE by query size (dimensions are opposite of
    /// what is stored in the BlastScoreBlk) containing frequency ratios
    /// Throws std::runtime_error if frequency ratios are not available
    static CNcbiMatrix<double>* 
    GetFreqRatios(CConstRef<objects::CPssmWithParameters> pssm);
};

END_SCOPE(blast)
END_NCBI_SCOPE

/* @} */

#endif  /* ALGO_BLAST_API___PSIBLAST_AUX_PRIV__HPP */
