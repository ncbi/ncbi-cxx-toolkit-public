#ifndef ALGO_BLAST_API___RPSTBLASTN_OPTIONS__HPP
#define ALGO_BLAST_API___RPSTBLASTN_OPTIONS__HPP

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
 * Authors:  Jason Papadopoulos
 *
 */

/// @file rpstblastn_options.hpp
/// Declares the CRPSTBlastnOptionsHandle class.

#include <algo/blast/api/blast_rps_options.hpp>

/** @addtogroup AlgoBlast
 *
 * @{
 */

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(blast)

/// Handle to the options for translated nucleotide-RPS blast
///
/// Adapter class for translated nucleotide - RPS BLAST searches.
/// Exposes an interface to allow manipulation the options that are relevant to
/// this type of search.

class NCBI_XBLAST_EXPORT CRPSTBlastnOptionsHandle : 
                                            public CBlastRPSOptionsHandle
{
public:

    /// Creates object with default options set
    CRPSTBlastnOptionsHandle(EAPILocality locality = CBlastOptions::eLocal);
    ~CRPSTBlastnOptionsHandle() {}

    /******************* Subject sequence options *******************/
    /// Returns DbGeneticCode
    int GetDbGeneticCode() const {
        return m_Opts->GetDbGeneticCode();
    }
    /// Sets DbGeneticCode
    /// @param gc DbGeneticCode [in]
    void SetDbGeneticCode(int gc) {
        m_Opts->SetDbGeneticCode(gc);
    }

protected:
    /// Set the program and service name for remote blast.
    virtual void SetRemoteProgramAndService_Blast3()
    {
        m_Opts->SetRemoteProgramAndService_Blast3("tblastn", "rpsblast");
    }
    
    /// Overrides SubjectSequenceOptionsDefaults for RPS-TBLASTN options
    void SetQueryOptionDefaults();

private:
    /// Disallow copy constructor
    CRPSTBlastnOptionsHandle(const CRPSTBlastnOptionsHandle& rhs);
    /// Disallow assignment operator
    CRPSTBlastnOptionsHandle& operator=(const CRPSTBlastnOptionsHandle& rhs);
};

END_SCOPE(blast)
END_NCBI_SCOPE


/* @} */

/*
 * =======================================================================
 * $Log$
 * Revision 1.6  2005/05/09 20:08:48  bealer
 * - Add program and service strings to CBlastOptions for remote blast.
 * - New CBlastOptionsHandle constructor for CRemoteBlast.
 * - Prohibit copy construction/assignment for CRemoteBlast.
 * - Code in each BlastOptionsHandle derived class to set program+service.
 *
 * Revision 1.5  2004/09/21 13:49:52  dondosha
 * RPS tblastn now needs a special SetQueryOptionDefaults method, but can use the default SetSubjectSequenceOptionsDefaults method
 *
 * Revision 1.4  2004/06/08 22:41:04  camacho
 * Add missing doxygen comments
 *
 * Revision 1.3  2004/05/04 13:09:20  camacho
 * Made copy-ctor & assignment operator private
 *
 * Revision 1.2  2004/04/23 13:55:47  papadopo
 * derived from BlastRPSOptionsHandle
 *
 * Revision 1.1  2004/04/16 14:08:30  papadopo
 * options handle for translated RPS blast
 *
 * =======================================================================
 */

#endif  /* ALGO_BLAST_API___RPSTBLASTN_OPTIONS__HPP */
