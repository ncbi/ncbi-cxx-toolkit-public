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
 * C++ API for the PSI-BLAST PSSM generation engine.
 *
 * Notes:
 * Variants of PSI-BLAST:
 *     PSI-BLAST: protein/pssm vs. protein db/sequence
 *     PSI-TBLASTN: protein/pssm vs. translated nucleotide db/sequence
 *
 * Binaries which run the variants mentioned above as implemented in C toolkit:
 *     blastpgp runs PSI-BLAST (-j 1 = blastp; -j n, n>1 = PSI-BLAST)
 *     blastall runs PSI-TBLASTN and it is only possible to run with a PSSM 
 *     from checkpoint file via the -R option and for only one iteration.
 *     PSI-TBLASTN cannot be run from just an iteration, it is always product
 *     of a restart with a checkpoint file.
 */
 
#include <corelib/ncbiobj.hpp>
#include <algo/blast/api/pssm_input.hpp>

class CPssmEngineTest;   // forward declaration of unit test class

BEGIN_NCBI_SCOPE

BEGIN_SCOPE(objects)
    // forward declaration of ASN.1 object containing PSSM
    class CScore_matrix_parameters;
END_SCOPE(objects)

BEGIN_SCOPE(blast)

class CPssmEngine
{
public:
    /// Does not take ownership of input
    CPssmEngine(IPssmInputData* input);

    CRef<objects::CScore_matrix_parameters> Run();

protected:
    /// Handle to strategy to process raw PSSM input data
    IPssmInputData*         m_PssmInput;

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

/*
 * ===========================================================================
 *
 * $Log$
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
