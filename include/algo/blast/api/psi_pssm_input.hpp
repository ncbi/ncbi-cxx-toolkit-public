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
#include <objmgr/scope.hpp>

/// Forward declaration for unit test classes
class CPssmCreateTestFixture;

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
#endif /* SKIP_DOXYGEN_PROCESSING */
END_SCOPE(objects)

BEGIN_SCOPE(blast)

/// Implements the interface to retrieve data for the last 2 stages of the PSSM
/// creation. Note that none of the data is owned by this class, it simply
/// returns pointers to the data it is passed
class NCBI_XBLAST_EXPORT CPsiBlastInputFreqRatios : public IPssmInputFreqRatios
{
public:
    /** 
     * @brief Constructor
     * 
     * @param query pointer to query sequence data [in]
     * @param query_length length of the query sequence data [in]
     * @param freq_ratios matrix of frequency ratios [in]
     * @param matrix_name name of underlying scoring matrix used [in]
     * @param gap_existence cost to open a gap, if zero default from IPssmInputData used.
     * @param gap_extension cost to open a gap, if zero default from IPssmInputData used.
     */
    CPsiBlastInputFreqRatios(const unsigned char* query,
                             unsigned int query_length,
                             const CNcbiMatrix<double>& freq_ratios,
                             const char* matrix_name = NULL,
                             int gap_existence = 0,
                             int gap_extension = 0)
        : m_Query(const_cast<unsigned char*>(query)), 
          m_QueryLength(query_length),
          m_MatrixName(matrix_name),
          m_GapExistence(gap_existence),
          m_GapExtension(gap_extension),
          m_FreqRatios(freq_ratios)
    {}

    /// No-op as we assume the data is passed in to the constructor
    void Process() {}

    /// @inheritDoc
    unsigned char* GetQuery() { return m_Query; }

    /// Get the query's length
    unsigned int GetQueryLength() { return m_QueryLength; }

    /// Obtain the name of the underlying matrix to use when building the PSSM
    const char* GetMatrixName() {
        return m_MatrixName ? m_MatrixName :
            IPssmInputFreqRatios::GetMatrixName();
    }

   /// Obtain the gap existence value to use when building the PSSM
    int GetGapExistence() {
         return m_GapExistence
         ? m_GapExistence
         : IPssmInputFreqRatios::GetGapExistence();
    }

    /// Obtain the gap extension value to use when building the PSSM
    int GetGapExtension() {
         return m_GapExtension
         ? m_GapExtension
         : IPssmInputFreqRatios::GetGapExtension();
    }

    /// Obtain a matrix of frequency ratios with this->GetQueryLength() columns
    /// and BLASTAA_SIZE rows
    const CNcbiMatrix<double>& GetData() {
        return m_FreqRatios;
    }

private:
    /// Query sequence data
    unsigned char*      m_Query;
    /// Length of query sequence data
    unsigned int        m_QueryLength;
    /// Name of underlying scoring matrix
    const char*         m_MatrixName;
    /// Gap existence penalty
    int                 m_GapExistence;
    /// Gap extension penalty
    int                 m_GapExtension;
    /// Frequency ratios
    CNcbiMatrix<double> m_FreqRatios;
};

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
    /// @param gap_existence cost to open a gap, if zero default from IPssmInputData used.
    /// @param gap_extension cost to open a gap, if zero default from IPssmInputData used.
    /// @param diags diagnostics data requests for the PSSM engine
    CPsiBlastInputData(const unsigned char* query,
                       unsigned int query_length,
                       CConstRef<objects::CSeq_align_set> sset,
                       CRef<objects::CScope> scope,
                       const PSIBlastOptions& opts,
                       const char* matrix_name = NULL,
                       int gap_existence = 0,
                       int gap_opening = 0,
                       const PSIDiagnosticsRequest* diags = NULL,
                       const string& query_title = "");

    /// virtual destructor
    virtual ~CPsiBlastInputData();

    /// The work to process the alignment is done here
    void Process();

    /// Get the query sequence used as master for the multiple sequence
    /// alignment in ncbistdaa encoding.
    unsigned char* GetQuery();

    /// Get the query's length
    unsigned int GetQueryLength();

    /// Obtain the multiple sequence alignment structure
    PSIMsa* GetData();

    /// Obtain the options for the PSSM engine
    const PSIBlastOptions* GetOptions();

    /// Obtain the name of the underlying matrix to use when building the PSSM
    const char* GetMatrixName();

   /// Obtain the gap existence value to use when building the PSSM
    int GetGapExistence() {
         return m_GapExistence
         ? m_GapExistence
         : IPssmInputData::GetGapExistence();
    }

    /// Obtain the gap extension value to use when building the PSSM
    int GetGapExtension() {
         return m_GapExtension
         ? m_GapExtension
         : IPssmInputData::GetGapExtension();
    }

    /// Obtain the diagnostics data that is requested from the PSSM engine
    const PSIDiagnosticsRequest* GetDiagnosticsRequest();

    /// @inheritDoc
    CRef<objects::CBioseq> GetQueryForPssm() {
        return m_QueryBioseq;
    }

private:

    /// Pointer to query sequence
    unsigned char*                  m_Query;
    /// Title of query
    string                          m_QueryTitle;
    /// Scope where to retrieve the sequences in the aligment from
    CRef<objects::CScope>           m_Scope;
    /// Structure representing the multiple sequence alignment
    PSIMsa*                         m_Msa;
    /// Multiple sequence alignment dimensions
    PSIMsaDimensions                m_MsaDimensions;
    /// Pairwise alignment result of a BLAST search
    CConstRef<objects::CSeq_align_set>   m_SeqAlignSet;
    /// Algorithm options
    PSIBlastOptions                 m_Opts;
    /// Diagnostics request structure
    PSIDiagnosticsRequest*          m_DiagnosticsRequest;
    /// Underlying matrix to use
    string                          m_MatrixName;
    /// Gap existence paramter used.
    int                             m_GapExistence;
    /// Gap extension paramter used.
    int                             m_GapExtension;
    /// Query as CBioseq for PSSM
    CRef<objects::CBioseq>          m_QueryBioseq;

    /////////////////////////// Auxiliary functions ///////////////////////////

    /// Tries to fetch the sequence data for the subject for the segments 
    /// specified in the Dense-seg. If the sequence cannot be retrieved from the
    /// scope, a warning is printed and an empty string is returned in
    /// sequence_data
    /// @param ds dense seg for which the sequence data is needed [in]
    /// @param scope scope from which to obtain the sequence data [in]
    /// @param sequence_data string which will contain the sequence data.
    static void
    x_GetSubjectSequence(const objects::CDense_seg& ds, objects::CScope& scope,
                         string& sequence_data);

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
    /// @param evalue evalue for this sequence aligned with the query (used for
    /// debugging only) [in]
    /// @param bit_score bit score for this sequence aligned with the query
    /// (used for debugging only) [in]
    void x_ProcessDenseg(const objects::CDense_seg& denseg, 
                         unsigned int msa_index,
                         double evalue, double bit_score);

    /// Extracts the query bioseq from m_SeqAlignSet
    void x_ExtractQueryForPssm();

    /// unit test class
    friend class ::CPssmCreateTestFixture;

private:
    /// prohibit copy constructor
    CPsiBlastInputData(const CPsiBlastInputData&);
    /// prohibit assignment operator
    CPsiBlastInputData& operator=(const CPsiBlastInputData&);
};

END_SCOPE(blast)
END_NCBI_SCOPE

/* @} */

#endif  /* ALGO_BLAST_API__PSI_PSSM_INPUT_HPP */
