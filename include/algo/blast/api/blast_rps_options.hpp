#ifndef ALGO_BLAST_API___BLAST_RPS_OPTIONS__HPP
#define ALGO_BLAST_API___BLAST_RPS_OPTIONS__HPP

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
 * Authors:  Tom Madden
 *
 */

/// @file blast_rps_options.hpp
/// Declares the CBlastRPSOptionsHandle class.


#include <algo/blast/api/blast_prot_options.hpp>

/** @addtogroup AlgoBlast
 *
 * @{
 */

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(blast)

/// Handle to the rpsblast options to the BLAST algorithm.
///
/// Adapter class for rpsblast BLAST comparisons.
/// Exposes an interface to allow manipulation the options that are relevant to
/// this type of search.

class NCBI_XBLAST_EXPORT CBlastRPSOptionsHandle : 
                                            public CBlastProteinOptionsHandle
{
public:
    
    /// Creates object with default options set
    CBlastRPSOptionsHandle(EAPILocality locality = CBlastOptions::eLocal);
    ~CBlastRPSOptionsHandle() {}
    CBlastRPSOptionsHandle(const CBlastRPSOptionsHandle& rhs);
    CBlastRPSOptionsHandle& operator=(const CBlastRPSOptionsHandle& rhs);

    /******************* Initial word options ***********************/
    int GetWindowSize() const { return m_Opts->GetWindowSize(); }
    void SetWindowSize(int ws) { m_Opts->SetWindowSize(ws); }

    double GetXDropoff() const { return m_Opts->GetXDropoff(); } 
    void SetXDropoff(double x) { m_Opts->SetXDropoff(x); }

    /******************* Gapped extension options *******************/
    double GetGapXDropoffFinal() const { 
        return m_Opts->GetGapXDropoffFinal(); 
    }
    void SetGapXDropoffFinal(double x) { m_Opts->SetGapXDropoffFinal(x); }

    /****************** Effective Length options ********************/

    Int8 GetEffectiveSearchSpace() const { 
        return m_Opts->GetEffectiveSearchSpace(); 
    }
    void SetEffectiveSearchSpace(Int8 eff) {
        m_Opts->SetEffectiveSearchSpace(eff);
    }

    bool GetUseRealDbSize() const { return m_Opts->GetUseRealDbSize(); }
    void SetUseRealDbSize(bool u = true) { m_Opts->SetUseRealDbSize(u); }

protected:
    void SetLookupTableDefaults();
    void SetQueryOptionDefaults();
};

END_SCOPE(blast)
END_NCBI_SCOPE


/* @} */


/*
 * ===========================================================================
 * $Log$
 * Revision 1.3  2004/04/16 14:31:49  papadopo
 * make this class a derived class of CBlastProteinOptionsHandle, that corresponds to the eRPSBlast program
 *
 * Revision 1.2  2004/03/19 14:53:24  camacho
 * Move to doxygen group AlgoBlast
 *
 * Revision 1.1  2004/03/10 14:52:34  madden
 * Options handle for RPSBlast searches
 *
 *
 * ===========================================================================
 */

#endif  /* ALGO_BLAST_API___BLAST_RPS_OPTIONS__HPP */
