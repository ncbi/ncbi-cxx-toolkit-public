#ifndef ALGO_BLAST_API__BLAST_PSI__HPP
#define ALGO_BLAST_API__BLAST_PSI__HPP

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

/** @file blast_psi.hpp
 * C++ API for the PSI-BLAST PSSM engine. 
 */
 
#include <corelib/ncbiobj.hpp>
#include <algo/blast/api/blast_aux.hpp>
#include <algo/blast/api/pssm_input.hpp>

// Forward declarations
class CPssmEngineTest;      // unit test class

/** @addtogroup AlgoBlast
 *
 * @{
 */

BEGIN_NCBI_SCOPE

BEGIN_SCOPE(objects)
    /// forward declaration of ASN.1 object containing PSSM (scoremat.asn)
    class CPssmWithParameters;
END_SCOPE(objects)

BEGIN_SCOPE(blast)

/// Computes a PSSM as specified in PSI-BLAST.
///
/// This class must be configured with a concrete strategy for it to obtain 
/// its input data.
/// The following example uses the CPsiBlastInputData concrete strategy:
///
/// @code
/// ...
/// CPsiBlastInputData pssm_strategy(query, query_length, alignment, 
///                                  object_manager_scope, psi_blast_options);
/// CPssmEngine pssm_engine(&pssm_strategy);
/// CRef<CPssmWithParameters> scoremat = pssm_engine.Run();
/// ...
/// @endcode

class NCBI_XBLAST_EXPORT CPssmEngine
{
public:
    /// Constructor to configure the PSSM engine with a PSSM input data
    /// strategy object
    CPssmEngine(IPssmInputData* input);
    ~CPssmEngine();

    /// Runs the PSSM engine to compute the PSSM
    CRef<objects::CPssmWithParameters> Run();

private:
    /// Handle to strategy to process raw PSSM input data
    IPssmInputData*         m_PssmInput;
    /// Blast score block structure
    CBlastScoreBlk          m_ScoreBlk;

    /// Copies query sequence and adds protein sentinel bytes at the beginning
    /// and at the end of the sequence.
    /// @param query sequence to copy [in]
    /// @param query_length length of the sequence above [in]
    /// @throws CBlastException if does not have enough memory
    /// @return copy of query guarded by protein sentinel bytes
    static unsigned char*
    x_GuardProteinQuery(const unsigned char* query,
                        unsigned int query_length);

    /// Initialiazes the core BlastQueryInfo structure for a single protein
    /// sequence.
    /// @todo this should be moved to the core of BLAST
    /// @param query_length length of the sequence above [in]
    BlastQueryInfo*
    x_InitializeQueryInfo(unsigned int query_length);

    /// Initializes the BlastScoreBlk required to run the PSSM engine.
    /// @todo this should be moved to the core of BLAST
    /// @param query sequence [in]
    /// @param query_length length of the sequence above [in]
    /// @param matrix_name name of the underlying scoring matrix to use [in]
    /// @throws CBlastException if does not have enough memory or if there was
    /// an error when setting up the return value
    /// @return initialized BlastScoreBlk
    BlastScoreBlk*
    x_InitializeScoreBlock(const unsigned char* query,
                           unsigned int query_length,
                           const char* matrix_name);

    /// Checks that no data returned by the IPssmInputData interface is NULL
    /// @throws CBlastException if validation fails. Does not test the
    /// GetData() method as this is only populated after Process() is called.
    void
    x_CheckAgainstNullData();

    /// Performs validation on data provided before invoking the CORE PSSM
    /// engine. Should be called after invoking Process() on the
    /// IPssmInputData.
    /// @throws CBlastException if validation fails
    void
    x_Validate();

    /// Converts the PSIMatrix structure into a ASN.1 CPssmWithParameters object
    /// @param pssm input PSIMatrix structure [in]
    /// @param opts options to be used in the PSSM engine [in]
    /// @param matrix_name name of the underlying scoring matrix used
    /// @param diagnostics contains diagnostics data from PSSM creation process
    /// to save into the return value [in]
    /// @return CPssmWithParameters object with equivalent contents to
    /// those in pssm argument
    static CRef<objects::CPssmWithParameters>
    x_PSIMatrix2Asn1(const PSIMatrix* pssm,
                     const PSIBlastOptions* opts,
                     const char* matrix_name,
                     const PSIDiagnosticsResponse* diagnostics = NULL);

    /// Convert a PSSM return status into a string
    /// @param error_code return value of a PSSM engine function as defined in
    /// blast_psi_priv.h [in]
    /// @return string containing a description of the error
    static std::string
    x_ErrorCodeToString(int error_code);

    /// Default constructor available for derived test classes
    CPssmEngine() {}
    /// Prohibit copy constructor
    CPssmEngine(const CPssmEngine& rhs);
    /// Prohibit assignment operator
    CPssmEngine& operator=(const CPssmEngine& rhs);

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
 * Revision 1.23  2005/05/03 20:32:52  camacho
 * Minor
 *
 * Revision 1.22  2005/03/28 18:27:35  jcherry
 * Added export specifiers
 *
 * Revision 1.21  2005/03/22 15:32:22  camacho
 * Minor change
 *
 * Revision 1.20  2005/03/07 17:00:07  camacho
 * Fix includes
 *
 * Revision 1.19  2005/03/07 15:20:36  camacho
 * fix forward declaration
 *
 * Revision 1.18  2004/12/13 23:07:24  camacho
 * Remove validation functions moved to algo/blast/core
 *
 * Revision 1.17  2004/11/22 14:38:06  camacho
 * + option to set % identity threshold to PSSM engine
 *
 * Revision 1.16  2004/11/02 20:37:16  camacho
 * Doxygen fixes
 *
 * Revision 1.15  2004/10/13 20:48:50  camacho
 * + support for requesting diagnostics information and specifying underlying matrix
 *
 * Revision 1.14  2004/10/13 15:44:21  camacho
 * + validation for columns in multiple sequence alignment
 *
 * Revision 1.13  2004/10/12 21:22:00  camacho
 * + validation methods
 *
 * Revision 1.12  2004/10/12 14:18:31  camacho
 * Update for scoremat.asn reorganization
 *
 * Revision 1.11  2004/09/17 13:11:20  camacho
 * Added doxygen comments
 *
 * Revision 1.10  2004/08/05 18:02:13  camacho
 * Enhanced documentation
 *
 * Revision 1.9  2004/08/04 20:52:37  camacho
 * Documentation changes
 *
 * Revision 1.8  2004/08/04 20:20:34  camacho
 * 1. Use CBlastScoreBlk instead of bare BlastScoreBlk pointer.
 * 2. Pass matrix name to properly populate Score_matrix::freq-Ratios field.
 *
 * Revision 1.7  2004/08/02 13:28:04  camacho
 * +Initialization of BlastScoreBlk and population of Score_matrix structure
 *
 * Revision 1.6  2004/07/29 17:53:16  camacho
 * Redesigned to use strategy pattern
 *
 * Revision 1.5  2004/06/16 18:44:00  ucko
 * Make sure to supply full declarations for C(Const)Refs' parameters,
 * as forward declarations are not sufficient for all compilers.
 *
 * Revision 1.4  2004/06/16 12:17:54  camacho
 * Fix for unit tests
 *
 * Revision 1.3  2004/06/08 22:27:44  camacho
 * Add missing doxygen comments
 *
 * Revision 1.2  2004/05/28 17:32:32  ucko
 * Remove redundant ncbi:: from specialization of CDeleter, as it
 * confuses some compilers.
 *
 * Revision 1.1  2004/05/28 16:39:42  camacho
 * Initial revision
 *
 *
 * ===========================================================================
 */

#endif  /* ALGO_BLAST_API__BLAST_PSI__HPP */
