#ifndef ALGO_BLAST_API___BLAST_PROT_OPTIONS__HPP
#define ALGO_BLAST_API___BLAST_PROT_OPTIONS__HPP

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

/// @file blast_prot_options.hpp
/// Declares the CBlastProteinOptionsHandle class.


#include <algo/blast/api/blast_options_handle.hpp>

/** @addtogroup AlgoBlast
 *
 * @{
 */

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(blast)

/// Handle to the protein-protein options to the BLAST algorithm.
///
/// Adapter class for protein-protein BLAST comparisons.
/// Exposes an interface to allow manipulation the options that are relevant to
/// this type of search.

class NCBI_XBLAST_EXPORT CBlastProteinOptionsHandle : 
                                            public CBlastOptionsHandle
{
public:
    
    /// Creates object with default options set
    CBlastProteinOptionsHandle(EAPILocality locality = CBlastOptions::eLocal);
    ~CBlastProteinOptionsHandle() {}

    /******************* Lookup table options ***********************/
    int GetWordThreshold() const { return m_Opts->GetWordThreshold(); }
    void SetWordThreshold(int wt) { m_Opts->SetWordThreshold(wt); }

    short GetWordSize() const { return m_Opts->GetWordSize(); }
    void SetWordSize(short ws) { m_Opts->SetWordSize(ws); }

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

    /************************ Scoring options ************************/
    const char* GetMatrixName() const { return m_Opts->GetMatrixName(); }
    void SetMatrixName(const char* matrix) { m_Opts->SetMatrixName(matrix); }

    const char* GetMatrixPath() const { return m_Opts->GetMatrixPath(); }
    void SetMatrixPath(const char* path) { m_Opts->SetMatrixPath(path); }

    int GetGapOpeningCost() const { return m_Opts->GetGapOpeningCost(); }
    void SetGapOpeningCost(int g) { m_Opts->SetGapOpeningCost(g); }

    int GetGapExtensionCost() const { return m_Opts->GetGapExtensionCost(); }
    void SetGapExtensionCost(int e) { m_Opts->SetGapExtensionCost(e); }

    Int8 GetEffectiveSearchSpace() const { 
        return m_Opts->GetEffectiveSearchSpace(); 
    }
    void SetEffectiveSearchSpace(Int8 eff) {
        m_Opts->SetEffectiveSearchSpace(eff);
    }

    bool GetUseRealDbSize() const { return m_Opts->GetUseRealDbSize(); }
    void SetUseRealDbSize(bool u = true) { m_Opts->SetUseRealDbSize(u); }

protected:
    virtual void SetLookupTableDefaults();
    virtual void SetQueryOptionDefaults();
    virtual void SetInitialWordOptionsDefaults();
    virtual void SetGappedExtensionDefaults();
    virtual void SetScoringOptionsDefaults();
    virtual void SetHitSavingOptionsDefaults();
    virtual void SetEffectiveLengthsOptionsDefaults();
    virtual void SetSubjectSequenceOptionsDefaults(); 

private:
    CBlastProteinOptionsHandle(const CBlastProteinOptionsHandle& rhs);
    CBlastProteinOptionsHandle& operator=(const CBlastProteinOptionsHandle& rhs);
};

END_SCOPE(blast)
END_NCBI_SCOPE


/* @} */


/*
 * ===========================================================================
 * $Log$
 * Revision 1.7  2004/05/04 13:09:20  camacho
 * Made copy-ctor & assignment operator private
 *
 * Revision 1.6  2004/03/19 14:53:24  camacho
 * Move to doxygen group AlgoBlast
 *
 * Revision 1.5  2004/03/10 14:55:02  madden
 * Added methods for get/set matrix, matrix-path, gap-opening, gap-extension
 *
 * Revision 1.4  2004/01/16 20:44:08  bealer
 * - Add locality flag for options handle classes.
 *
 * Revision 1.3  2003/12/15 23:41:36  dondosha
 * Added [gs]etters of database (subject) length and number of sequences to general options handle
 *
 * Revision 1.2  2003/12/09 12:40:22  camacho
 * Added windows export specifiers
 *
 * Revision 1.1  2003/11/26 18:22:16  camacho
 * +Blast Option Handle classes
 *
 * ===========================================================================
 */

#endif  /* ALGO_BLAST_API___BLAST_PROT_OPTIONS__HPP */
