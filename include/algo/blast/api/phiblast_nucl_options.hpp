#ifndef ALGO_BLAST_API___PHIBLAST_NUCL_OPTIONS__HPP
#define ALGO_BLAST_API___PHIBLAST_NUCL_OPTIONS__HPP

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
 * Authors:  Ilya Dondoshansky
 *
 */

/// @file phiblast_nucl_options.hpp
/// Declares the CPHIBlastNuclOptionsHandle class.


#include <algo/blast/api/blast_nucl_options.hpp>

/** @addtogroup AlgoBlast
 *
 * @{
 */

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(blast)

/// Handle to the nuclein PHI BLAST options.
///
/// Adapter class for PHI BLAST search.
/// Exposes an interface to allow manipulation the options that are relevant to
/// this type of search.

class NCBI_XBLAST_EXPORT CPHIBlastNuclOptionsHandle : public CBlastNucleotideOptionsHandle
{
public:
    
    /// Creates object with default options set
    CPHIBlastNuclOptionsHandle(EAPILocality locality = CBlastOptions::eLocal);
    ~CPHIBlastNuclOptionsHandle() {}
    
    /******************* PHI options ***********************/
    const char* GetPHIPattern() const;
    void SetPHIPattern(const char* p);

private:
    /// Disallow copy constructor
    CPHIBlastNuclOptionsHandle(const CPHIBlastNuclOptionsHandle& rhs);
    /// Disallow assignment operator
    CPHIBlastNuclOptionsHandle& operator=(const CPHIBlastNuclOptionsHandle& rhs);
};

/// Retrieves the pattern string option
/// @return Pattern string satisfying PROSITE rules.
inline const char* CPHIBlastNuclOptionsHandle::GetPHIPattern() const
{
    return m_Opts->GetPHIPattern();
}

/// Sets the pattern string option
/// @param pattern The pattern string satisfying PROSITE rules.
inline void CPHIBlastNuclOptionsHandle::SetPHIPattern(const char* pattern)
{
    m_Opts->SetPHIPattern(pattern, true);
}

END_SCOPE(blast)
END_NCBI_SCOPE


/* @} */


/*
 * ===========================================================================
 * $Log$
 * Revision 1.1  2005/05/26 14:35:30  dondosha
 * PHI BLAST options handle classes
 *
 *
 * ===========================================================================
 */

#endif  /* ALGO_BLAST_API___PHIBLAST_NUCL_OPTIONS__HPP */
