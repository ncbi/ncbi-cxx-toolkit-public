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
#include <algo/blast/api/pssm_input.hpp>

// Forward declarations
struct BlastScoreBlk;       // Core BLAST scoring block structure
class CPssmEngineTest;      // unit test class

/** @addtogroup AlgoBlast
 *
 * @{
 */

BEGIN_NCBI_SCOPE

BEGIN_SCOPE(objects)
    // forward declaration of ASN.1 object containing PSSM
    class CScore_matrix_parameters;
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
/// CRef<CScore_matrix_parameters> scoremat = pssm_engine.Run();
/// ...
/// @endcode

class CPssmEngine
{
public:
    /// Constructor to configure the PSSM engine with a PSSM input data
    /// strategy object
    CPssmEngine(IPssmInputData* input);
    ~CPssmEngine();

    /// Runs the PSSM engine to compute the PSSM
    CRef<objects::CScore_matrix_parameters> Run();

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
    unsigned char*
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
    /// @throws CBlastException if does not have enough memory or if there was
    /// an error when setting up the return value
    /// @return initialized BlastScoreBlk
    BlastScoreBlk*
    x_InitializeScoreBlock(const unsigned char* query,
                           unsigned int query_length);

    /// Converts the PSIMatrix structure into a CScore_matrix_parameters object
    /// @param pssm input PSIMatrix structure [in]
    /// @param matrix_name underlying scoring matrix name [in]
    /// @return CScore_matrix_parameters object with equivalent contents to
    /// those in pssm argument
    CRef<objects::CScore_matrix_parameters>
    x_PSIMatrix2ScoreMatrix(const PSIMatrix* pssm,
                            const string& matrix_name,
                            const PSIDiagnosticsResponse* diagnostics = NULL);

    /// Default constructor available for derived test classes
    CPssmEngine() {}
    /// Prohibit copy constructor
    CPssmEngine(const CPssmEngine& rhs);
    /// Prohibit assignment operator
    CPssmEngine& operator=(const CPssmEngine& rhs);

    friend class ::CPssmEngineTest; // unit test class
};

END_SCOPE(blast)
END_NCBI_SCOPE

/* @} */

/*
 * ===========================================================================
 *
 * $Log$
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
