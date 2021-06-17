/*  $Id$
 * ===========================================================================
 *
 *                            PUBLIC DOMAIN NOTICE
 *
 * ===========================================================================
 *
 * Author: Robert M. Hubley
 *         Shamelessly based on the work of Christiam Camacho in 
 *         blastn_args.hpp
 *
 */

/** @file rmblastn_args.hpp
 * Main argument class for RMBLASTN application
 */

#ifndef ALGO_BLAST_BLASTINPUT___RMBLASTN_ARGS__HPP
#define ALGO_BLAST_BLASTINPUT___RMBLASTN_ARGS__HPP

#include <algo/blast/blastinput/blast_args.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(blast)

/// Handles command line arguments for blastn binary
class NCBI_BLASTINPUT_EXPORT CRMBlastnAppArgs : public CBlastAppArgs
{
public:
    /// Constructor
    CRMBlastnAppArgs();

    virtual int GetQueryBatchSize() const;

protected:
    virtual CRef<CBlastOptionsHandle>
    x_CreateOptionsHandle(CBlastOptions::EAPILocality locality,
                          const CArgs& args);

};


END_SCOPE(blast)
END_NCBI_SCOPE

#endif  /* ALGO_BLAST_BLASTINPUT___BLASTN_ARGS__HPP */
