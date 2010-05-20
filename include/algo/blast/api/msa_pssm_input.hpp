#ifndef ALGO_BLAST_API__MSA_PSSM_INPUT__HPP
#define ALGO_BLAST_API__MSA_PSSM_INPUT__HPP

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

/** @file msa_pssm_input.hpp
 * Defines a concrete strategy to obtain PSSM input data for PSI-BLAST from a
 * multiple sequence alignment file.
 */

#include <corelib/ncbiobj.hpp>
#include <algo/blast/api/blast_aux.hpp>
#include <algo/blast/api/pssm_input.hpp>
#include <objects/seqset/Seq_entry.hpp>

/** @addtogroup AlgoBlast
 *
 * @{
 */

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(blast)

/// This class is a concrete strategy for IPssmInputData which converts the
/// CLUSTALW-style output containing a multiple sequence alignment into the data
/// structures needed by the PSSM engine.
class NCBI_XBLAST_EXPORT CPsiBlastInputClustalW: public IPssmInputData
{
public:
    /// Construct a concrete strategy, used to configure the CPssmEngine object
    /// @param input_file Input file containing the multiple sequence
    /// alignment. [in]
    /// @param opts options to be used in the PSSM engine
    /// @param matrix_name name of the substitution matrix to use to build PSSM
    /// If not provided, the default implementation of
    /// IPssmInputData::GetMatrixName() will be returned
    /// @param diags diagnostics data requests for the PSSM engine
    /// @param query query sequence for the alignment in ncbistdaa encoding.
    /// @param query_length length of the sequence above.
    /// @param gap_existence cost to open a gap, if zero default from IPssmInputData used.
    /// @param gap_extension cost to open a gap, if zero default from IPssmInputData used.
    CPsiBlastInputClustalW(CNcbiIstream& input_file,
                           const PSIBlastOptions& opts,
                           const char* matrix_name = NULL,
                           const PSIDiagnosticsRequest* diags = NULL,
                           const unsigned char* query = NULL,
                           unsigned int query_length = 0,
                           int gap_existence = 0,
                           int gap_opening = 0);

    /// virtual destructor
    virtual ~CPsiBlastInputClustalW();

    /// The work to process the alignment is done here
    void Process();

    /// Get the query sequence used as master for the multiple sequence
    /// alignment in ncbistdaa encoding.
    unsigned char* GetQuery() { return m_Query.get(); }

    /// Get the query's length
    unsigned int GetQueryLength() { return m_MsaDimensions.query_length; }

    /// Obtain the multiple sequence alignment structure
    PSIMsa* GetData() { return m_Msa; }

    /// Obtain the options for the PSSM engine
    const PSIBlastOptions* GetOptions() {
        return &m_Opts;
    }

    /// Obtain the name of the underlying matrix to use when building the PSSM
    const char* GetMatrixName() {
        return m_MatrixName.empty() 
            ? IPssmInputData::GetMatrixName()
            : m_MatrixName.c_str();
    }

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
    const PSIDiagnosticsRequest* GetDiagnosticsRequest() {
        return m_DiagnosticsRequest;
    }

    /// @inheritDoc
    CRef<objects::CBioseq> GetQueryForPssm() { 
        return m_QueryBioseq;
    }

private:

    /// Pointer to query sequence
    TAutoUint1ArrayPtr              m_Query;
    /// The raw multiple sequence alignment in ASCII read from the input file
    vector<string>                  m_AsciiMsa;
    /// Structure representing the multiple sequence alignment
    PSIMsa*                         m_Msa;
    /// Multiple sequence alignment dimensions
    PSIMsaDimensions                m_MsaDimensions;
    /// Algorithm options
    PSIBlastOptions                 m_Opts;
    /// Diagnostics request structure
    PSIDiagnosticsRequest*          m_DiagnosticsRequest;
    /// Underlying matrix to use
    string                          m_MatrixName;
    /// Gap existence parameter used.
    int                             m_GapExistence;
    /// Gap extension parameter used.
    int                             m_GapExtension; 
    /// CSeq_entry obtained from the multiple sequence alignment
    CRef<objects::CSeq_entry>       m_SeqEntry;
    /// Query as CBioseq for PSSM
    CRef<objects::CBioseq>          m_QueryBioseq;

    /////////////////////////// Auxiliary functions ///////////////////////////

    /// Reads the multiple sequence alignment from the input file
    /// @param input_file Input file containing the multiple sequence
    /// alignment. [in]
    /// @post m_AsciiMsa and m_SeqEntry are not empty
    void x_ReadAsciiMsa(CNcbiIstream& input_file);

    /// Extracts the query sequence from the multiple sequence alignment,
    /// assuming it's the first one, into m_Query
    /// @post m_Query is not NULL and m_MsaDimensions.query_length is assigned
    void x_ExtractQueryFromMsa();
    
    /// Searches the query sequence (m_Query) in the aligned sequences
    /// (m_AsciiMsa) and moves the first instance it finds to the front of this
    /// data structure.
    /// @throw CBlastException if the query sequence is not found.
    void x_ValidateQueryInMsa();

    /// Copies query sequence data to multiple alignment data structure
    void x_CopyQueryToMsa();

    /// Populates the multiple alignment data structure
    void x_ExtractAlignmentData();

    /// Extracts the query bioseq from m_SeqEntry
    void x_ExtractQueryForPssm();
private:
    /// prohibit copy constructor
    CPsiBlastInputClustalW(const CPsiBlastInputClustalW&);
    /// prohibit assignment operator
    CPsiBlastInputClustalW& operator=(const CPsiBlastInputClustalW&);
};

END_SCOPE(blast)
END_NCBI_SCOPE

/* @} */

#endif  /* ALGO_BLAST_API__MSA_PSSM_INPUT_HPP */
