#ifndef ALGO_BLAST_API__PSI_PSSM_INPUT__HPP
#define ALGO_BLAST_API__PSI_PSSM_INPUT__HPP

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

/** @file psi_pssm_input.hpp
 * Defines a concrete strategy to obtain PSSM input data for PSI-BLAST.
 */

#include <corelib/ncbiobj.hpp>
#include <algo/blast/api/blast_aux.hpp>
#include <algo/blast/api/pssm_input.hpp>

/// Forward declaration for unit test classes
class CPssmEngineTest;

/** @addtogroup AlgoBlast
 *
 * @{
 */

BEGIN_NCBI_SCOPE

// Forward declarations in objects scope
BEGIN_SCOPE(objects)
#ifndef SKIP_DOXYGEN_PROCESSING
    class CSeq_align_set;
    class CDense_seg;
    class CScope;
#endif /* SKIP_DOXYGEN_PROCESSING */
END_SCOPE(objects)

BEGIN_SCOPE(blast)

/// This class is a concrete strategy for IPssmInputData, and it
/// implements the traditional PSI-BLAST algorithm for building a multiple
/// sequence alignment from a list of pairwise alignments using the C++ object
/// manager.
class NCBI_XBLAST_EXPORT CPsiBlastInputData : public IPssmInputData
{
public:
    /// Construct a concrete strategy, used to configure the CPssmEngine object
    /// @param query query sequence for the alignment in ncbistdaa encoding.
    /// @param query_length length of the sequence above.
    /// @param sset pairwise alignment produced by BLAST where query was the
    /// query sequence.
    /// @param scope object manager scope from which to retrieve sequence data
    /// [in]
    /// @param opts options to be used in the PSSM engine
    /// @param matrix_name name of the substitution matrix to use to build PSSM
    /// If not provided, the default implementation of
    /// IPssmInputData::GetMatrixName() will be returned
    /// @param diags diagnostics data requests for the PSSM engine
    CPsiBlastInputData(const unsigned char* query,
                       unsigned int query_length,
                       CRef<objects::CSeq_align_set> sset,
                       CRef<objects::CScope> scope,
                       const PSIBlastOptions& opts,
                       const char* matrix_name = NULL,
                       const PSIDiagnosticsRequest* diags = NULL);
    virtual ~CPsiBlastInputData();

    void Process();
    unsigned char* GetQuery();
    unsigned int GetQueryLength();
    PSIMsa* GetData();
    const PSIBlastOptions* GetOptions();
    const char* GetMatrixName();
    const PSIDiagnosticsRequest* GetDiagnosticsRequest();

private:

    /// Pointer to query sequence
    unsigned char*                  m_Query;
    /// Scope where to retrieve the sequences in the aligment from
    CRef<objects::CScope>           m_Scope;
    /// Structure representing the multiple sequence alignment
    PSIMsa*                         m_Msa;
    /// Multiple sequence alignment dimensions
    PSIMsaDimensions                m_MsaDimensions;
    /// Pairwise alignment result of a BLAST search
    CRef<objects::CSeq_align_set>   m_SeqAlignSet;
    /// Algorithm options
    PSIBlastOptions                 m_Opts;
    /// Diagnostics request structure
    PSIDiagnosticsRequest*          m_DiagnosticsRequest;
    /// Underlying matrix to use
    string                          m_MatrixName;
    /// Helps to keep track of the hits to process because they meet the
    /// inclusion e-value threshold criteria
    std::vector<Uint1>              m_ProcessHit;

    /////////////////////////// Auxiliary functions ///////////////////////////

    /// Typedef for sequence data and its length
    typedef pair<TAutoUint1ArrayPtr, TSeqPos> TSeqPair;

    /// Tries to fetch the sequence data for the subject for the segments 
    /// specified in the Dense-seg. If the sequence cannot be retrieved from the
    /// scope, a warning is printed and the pair (NULL, 0) is returned.
    /// @param ds dense seg for which the sequence data is needed [in]
    /// @param scope scope from which to obtain the sequence data [in]
    static TSeqPair 
    x_GetSubjectSequence(const objects::CDense_seg& ds, objects::CScope& scope);

    /// Examines the sequence alignment and keeps track of those hits which
    /// have an HSP with an e-value below the inclusion threshold specified in
    /// the PSIBlastOptions structure.
    /// @return number of hits which qualify for constructing the multiple
    /// sequence alignment structure
    unsigned int
    x_CountAndSelectQualifyingAlignments();

    /// Populates the multiple alignment data structure
    void x_ExtractAlignmentData();
    // First implementation of use_best_align option from old toolkit. Should
    // be implemented as a subclass of this one?
    //void x_ExtractAlignmentDataUseBestAlign();

    /// Copies query sequence data to multiple alignment data structure
    void x_CopyQueryToMsa();

    /// Returns the number of sequences that make up the multiple sequence
    /// alignment
    /// @throws CBlastException if this number hasn't been calculated yet (need
    /// to invoke Process() first!)
    unsigned int GetNumAlignedSequences() const;

    /// Iterates over the Dense-seg passed in and extracts alignment 
    /// information to multiple alignment data structure.
    /// @param denseg source alignment segment (HSP) [in]
    /// @param msa_index index of the sequence aligned with the query in the
    ///        desc_matrix field of the m_AlignmentData data member [in]
    void x_ProcessDenseg(const objects::CDense_seg& denseg, 
                         unsigned int msa_index);

    /// unit test class
    friend class ::CPssmEngineTest;
};

END_SCOPE(blast)
END_NCBI_SCOPE

/* @} */

/*
 * ===========================================================================
 *
 * $Log$
 * Revision 1.11  2005/03/28 18:27:35  jcherry
 * Added export specifiers
 *
 * Revision 1.10  2005/03/07 17:00:07  camacho
 * Fix includes
 *
 * Revision 1.9  2004/12/28 16:45:57  camacho
 * Move typedefs to AutoPtr to public header so that they are used consistently
 *
 * Revision 1.8  2004/11/02 20:37:16  camacho
 * Doxygen fixes
 *
 * Revision 1.7  2004/10/13 20:48:50  camacho
 * + support for requesting diagnostics information and specifying underlying matrix
 *
 * Revision 1.6  2004/08/05 18:02:13  camacho
 * Enhanced documentation
 *
 * Revision 1.5  2004/08/04 21:13:11  camacho
 * Removed unneeded e_value parameter
 *
 * Revision 1.4  2004/08/04 20:52:37  camacho
 * Documentation changes
 *
 * Revision 1.3  2004/08/04 20:21:37  camacho
 * Renamed multiple sequence alignment data structure
 *
 * Revision 1.2  2004/08/02 13:29:49  camacho
 * + implementation query sequence methods
 *
 * Revision 1.1  2004/07/29 17:53:47  camacho
 * Initial revision
 *
 * ===========================================================================
 */
#endif  /* ALGO_BLAST_API__PSI_PSSM_INPUT_HPP */
