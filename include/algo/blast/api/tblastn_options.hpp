#ifndef ALGO_BLAST_API___TBLASTN_OPTIONS__HPP
#define ALGO_BLAST_API___TBLASTN_OPTIONS__HPP

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
 * Authors:  Christiam Camacho
 *
 */

/// @file tblastn_options.hpp
/// Declares the CTBlastnOptionsHandle class.

#include <algo/blast/api/blast_prot_options.hpp>

/** @addtogroup Miscellaneous
 *
 * @{
 */

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(blast)

/// Handle to the protein-translated nucleotide options to the BLAST algorithm.
///
/// Adapter class for protein-translated nucleotide BLAST comparisons.
/// Exposes an interface to allow manipulation the options that are relevant to
/// this type of search.

class CTBlastnOptionsHandle : public CBlastProteinOptionsHandle
{
public:

    /// Creates object with default options set
    CTBlastnOptionsHandle();
    ~CTBlastnOptionsHandle() {}
    CTBlastnOptionsHandle(const CTBlastnOptionsHandle& rhs);
    CTBlastnOptionsHandle& operator=(const CTBlastnOptionsHandle& rhs);

    /************************ Scoring options ************************/
    // is this needed or can we use a sentinel for the frame shift penalty?
    bool GetOutOfFrameMode() const { return m_Opts->GetOutOfFrameMode(); }
    void SetOutOfFrameMode(bool m = true) { m_Opts->SetOutOfFrameMode(m); }

    int GetFrameShiftPenalty() const { return m_Opts->GetFrameShiftPenalty(); }
    void SetFrameShiftPenalty(int p) { m_Opts->SetFrameShiftPenalty(p); }

    /******************* Subject sequence options *******************/
    const unsigned char* GetDbGeneticCodeStr() const {
        return m_Opts->GetDbGeneticCodeStr();
    }
    void SetDbGeneticCodeStr(const unsigned char* gc_str) {
        m_Opts->SetDbGeneticCodeStr(gc_str);
    }

protected:
    void SetLookupTableDefaults();
    void SetScoringOptionsDefaults();
    void SetHitSavingOptionsDefaults();
    void SetSubjectSequenceOptionsDefaults();
};

END_SCOPE(blast)
END_NCBI_SCOPE


/* @} */


/*
 * ===========================================================================
 * $Log$
 * Revision 1.1  2003/11/26 18:22:18  camacho
 * +Blast Option Handle classes
 *
 * ===========================================================================
 */

#endif  /* ALGO_BLAST_API___TBLASTN_OPTIONS__HPP */
