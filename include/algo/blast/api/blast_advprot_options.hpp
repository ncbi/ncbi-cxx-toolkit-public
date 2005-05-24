#ifndef ALGO_BLAST_API___ADVPROT_OPTIONS__HPP
#define ALGO_BLAST_API___ADVPROT_OPTIONS__HPP

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

/// @file blast_advprot_options.hpp
/// Declares the CBlastAdvancedProteinOptionsHandle class.
/// This class supports protein options such as composition-based stats and 
/// Smith-Waterman that are not implemented for blastx, tblastn, etc.


#include <algo/blast/api/blast_prot_options.hpp>

/** @addtogroup AlgoBlast
 *
 * @{
 */


BEGIN_NCBI_SCOPE
BEGIN_SCOPE(blast)

/// Handle to the Advanced BLASTP options.
///
/// Adapter class for advanced BLASTP options
/// Exposes an interface to allow manipulation the options that are relevant to
/// this type of search.

class NCBI_XBLAST_EXPORT CBlastAdvancedProteinOptionsHandle : 
                                            public CBlastProteinOptionsHandle
{
public:

    /// Creates object with default options set
    CBlastAdvancedProteinOptionsHandle(EAPILocality locality = CBlastOptions::eLocal);
    ~CBlastAdvancedProteinOptionsHandle() {}

/******************* Gapped extension options *******************/

    /// Returns this mode, which mostly specifies whether composition of db sequence is taken
    /// into account when calculating expect values.
    bool GetCompositionBasedStatsMode() const { return m_Opts->GetCompositionBasedStatsMode(); }

    /// Sets this mode, which mostly specifies whether composition of db sequence is taken
    /// into account when calculating expect values.
    /// @param m composition-based statistics mode [in]
    void SetCompositionBasedStatsMode(bool m = true)  { m_Opts->SetCompositionBasedStatsMode(m); }

    /// Returns this mode, specifying that smith waterman rather than the normal blast heuristic
    /// should be used for final extensions.
    /// into account when calculating expect values.
    bool GetSmithWatermanMode() const { return m_Opts->GetSmithWatermanMode(); }

    /// Sets this mode, specifying that smith waterman rather than the normal blast heuristic
    /// should be used for final extensions.
    /// @param m use smith-waterman if true [in]
    void SetSmithWatermanMode(bool m = false)  { m_Opts->SetSmithWatermanMode(m); }

protected:
    /// Overrides  SetGappedExtensionDefaults for advanced blastp options
    void  SetGappedExtensionDefaults();

private:
    /// Disallow copy constructor
    CBlastAdvancedProteinOptionsHandle(const CBlastAdvancedProteinOptionsHandle& rhs);
    /// Disallow assignment operator
    CBlastAdvancedProteinOptionsHandle& operator=(const CBlastAdvancedProteinOptionsHandle& rhs);
};

END_SCOPE(blast)
END_NCBI_SCOPE


/* @} */


/*
 * ===========================================================================
 * $Log$
 * Revision 1.1  2005/05/24 14:03:19  madden
 * Advanced (e.g., composition-based) protein options
 *
 *
 * ===========================================================================
 */

#endif  /* ALGO_BLAST_API___ADVPROT_OPTIONS__HPP */
